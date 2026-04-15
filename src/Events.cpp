#include "Events.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <shared_mutex>


// https://blackdoor.github.io/blog/thumbstick-controls/

static inline double GetAngle(RE::NiPoint2 a_vec)
{
	//Normally atan2 takes Y,X, not X,Y.  We switch these around since we want 0
	// degrees to be straight up, not to the right like the unit circle;
	double angleInRadians = std::atan2(a_vec.x, a_vec.y);

	//atan2 gives us a negative value for angles in the 3rd and 4th quadrants.
	// We want a full 360 degrees, so we will add 2 PI to negative values.
	if (angleInRadians < 0.0f)
		angleInRadians += (M_PI * 2.0f);

	//Convert the radians to degrees.  Degrees are easier to visualize.
	double angleInDegrees = (180.0f * angleInRadians / M_PI);

	return angleInDegrees;
}

static inline RE::hkbClipGenerator* ToClipGenerator(RE::hkbNode* a_node)
{
	// O skyrim_cast já faz a verificação de tipo e retorna nullptr se não for compatível
	if (a_node) {
		return skyrim_cast<RE::hkbClipGenerator*>(a_node);
	}

	return nullptr;
}

static inline bool HasSCARComboEvent(RE::Actor* a_actor)
{
	if (!a_actor)
		return false;

	RE::BSAnimationGraphManagerPtr graphMgr;
	if (a_actor->GetAnimationGraphManager(graphMgr) && graphMgr) {
		auto behaviourGraph = graphMgr->graphs[0] ? graphMgr->graphs[0]->behaviorGraph : nullptr;
		auto activeNodes = behaviourGraph ? behaviourGraph->activeNodes : nullptr;
		if (activeNodes) {
			for (auto nodeInfo : *activeNodes) {
				auto nodeClone = nodeInfo.nodeClone;
				if (nodeClone && nodeClone->GetClassType()) {
					auto clipGenrator = ToClipGenerator(nodeClone);
					if (clipGenrator) {
						auto binding = clipGenrator->binding;
						auto animation = binding ? binding->animation : nullptr;
						if (animation && !animation->annotationTracks.empty()) {
							for (auto anno : animation->annotationTracks[0].annotations) {
								std::string_view text{ anno.text.c_str() };
								if (text.starts_with("SCAR_ComboStart"))
									return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

BFCO::Direction BFCO::GetDirection(RE::NiPoint2 a_vec, bool a_gamepad)
{
	if (a_vec.Length() > (a_gamepad ? 0.25f : 0.0f)) {
		//We have 4 sectors, so get the size of each in degrees.
		constexpr double sectorSize = 360.0f / 4;

		//We also need the size of half a sector
		constexpr double halfSectorSize = sectorSize / 2.0f;

		//First, get the angle using the function above
		double angle = GetAngle(a_vec);

		//Next, rotate our angle to match the offset of our sectors.
		double convertedAngle = angle + halfSectorSize;

		//Finally, we get the current direction by dividing the angle
		// by the size of the sectors
		Direction direction = (Direction)(std::floor(convertedAngle / sectorSize) + 1);

		//the result directions map as follows:
		// 0 = kForward, 1 = kStrafeRight, 2 = kBack 3 = kStrafeLeft.
		return direction;
	}
	//DEBUG("kNeutral");
	return Direction::kNeutral;
}

BFCO::DirectionOcto BFCO::GetDirectionOcto(RE::NiPoint2 a_vec, bool a_gamepad)
{
	if (a_vec.Length() > (a_gamepad ? 0.25f : 0.0f)) {
		//We have 8 sectors, so get the size of each in degrees.
		constexpr double sectorSize = 360.0f / 8;

		//We also need the size of half a sector
		constexpr double halfSectorSize = sectorSize / 2.0f;

		//First, get the angle using the function above
		double angle = GetAngle(a_vec);

		//Next, rotate our angle to match the offset of our sectors.
		double convertedAngle = angle + halfSectorSize;

		//Finally, we get the current direction by dividing the angle
		// by the size of the sectors
		DirectionOcto direction = (DirectionOcto)(std::floor(convertedAngle / sectorSize) + 1);

		//the result directions map as follows:
		// 0 = kForward, 1 = kForwardRight, 2 = kStrafeRight ... 7 = kForwardLeft.
		return direction;
	}
	//DEBUG("kNeutral");
	return DirectionOcto::kNeutral;
}

void BFCO::ProcessMovement(RE::PlayerControlsData* a_data, bool a_gamepad)
{
	if (auto player = RE::PlayerCharacter::GetSingleton()) {
		if (GetDirectionOcto(a_data->moveInputVec, a_gamepad) != DirectionOcto::kNeutral) {
			player->NotifyAnimationGraph("BFCO_MoveStart");
		}
	}
}

inline RE::bhkRigidBody* GetRigidBody(const RE::TESObjectREFR* refr) {
	const auto object3D = refr->Get3D();

	if (!object3D) {
		return nullptr;
	}
	const auto collision = object3D->GetCollisionObject();

	if (!collision) {
		return nullptr;
	}

	const auto body = collision->GetRigidBody();

	return body;
}

inline RE::NiPoint3 GetPosition(const RE::TESObjectREFR* obj) {
	const auto body = GetRigidBody(obj);
	if (!body) return obj->GetPosition();
	RE::hkVector4 havockPosition;
	body->GetPosition(havockPosition);
	float components[4];
	_mm_store_ps(components, havockPosition.quad);
	RE::NiPoint3 newPosition = { components[0], components[1], components[2] };
	constexpr float havockToSkyrimConversionRate = 69.9915f;
	newPosition *= havockToSkyrimConversionRate;
	return newPosition;
}



namespace BFCOCombatState {
	inline std::unordered_map<RE::FormID, bool> LastWasPower;
	inline std::shared_mutex StateMutex;

	inline void SetLastWasPower(RE::FormID actorID, bool wasPower) {
		std::unique_lock lock(StateMutex);
		LastWasPower[actorID] = wasPower;
	}

	inline bool GetLastWasPower(RE::FormID actorID) {
		std::shared_lock lock(StateMutex);
		auto it = LastWasPower.find(actorID);
		if (it != LastWasPower.end()) {
			return it->second;
		}
		return false;
	}

	inline void ClearState(RE::FormID actorID) {
		std::unique_lock lock(StateMutex);
		LastWasPower.erase(actorID);
	}
}

namespace BFCOIdles {


	inline RE::TESIdleForm* GetIdleByFormID(RE::FormID formID, const std::string& pluginName) {
		if (auto dataHandler = RE::TESDataHandler::GetSingleton()) {
			auto form = dataHandler->LookupForm(formID, pluginName);
			if (form) return form->As<RE::TESIdleForm>();
		}
		return nullptr;
	}

	void InitIdles() {
		const std::string pluginName = "SCSI-ACTbfco-Main.esp";
		const std::string skyrim = "Skyrim.esm";

		AttackNormal = GetIdleByFormID(0x803, pluginName);
		PowerNormal = GetIdleByFormID(0x8C5, pluginName);
		PowerH2H = GetIdleByFormID(0x839, pluginName);
		PowerBash = GetIdleByFormID(0x8C0, pluginName);
		SprintPower = GetIdleByFormID(0x8BE, pluginName);
		ComboAttack = GetIdleByFormID(0x8BF, pluginName);

		JumpPower = GetIdleByFormID(0x944, pluginName);
		PowerDirA = GetIdleByFormID(0x945, pluginName);
		PowerDirB = GetIdleByFormID(0x946, pluginName);
		PowerDirL = GetIdleByFormID(0x947, pluginName);
		PowerDirR = GetIdleByFormID(0x948, pluginName);
		CancelDodge = GetIdleByFormID(0x949, pluginName);
		PowerRight = RE::TESForm::LookupByID<RE::BGSAction>(0xE8456);

	}

	inline void PlayIdleAnimation(RE::Actor* actor, RE::TESIdleForm* idle) {
		if (actor && idle) {
			if (auto* processManager = actor->GetActorRuntimeData().currentProcess) {
				processManager->PlayIdle(actor, idle, actor);
			}
			else {
				SKSE::log::error("Não foi possível obter o AIProcess (currentProcess) do ator.");
			}
		}
	}

	inline void PerformAction(RE::BGSAction* action, RE::Actor* player) {
		if (action && player) {
			std::unique_ptr<RE::TESActionData> data(RE::TESActionData::Create());
			data->source = RE::NiPointer<RE::TESObjectREFR>(player);
			data->action = action;
			typedef bool func_t(RE::TESActionData*);
			REL::Relocation<func_t> func{ RELOCATION_ID(40551, 41557) };
			func(data.get());
		}
	}
}


int ExtractComboNumber(std::string_view a_tag, std::string_view prefix) {
	if (a_tag.size() > prefix.size()) {
		std::string numStr(a_tag.substr(prefix.size()));
		try {
			return std::stoi(numStr);
		}
		catch (...) {
			return 0;
		}
	}
	return 0;
}

// NPC
void ProcessAttackWinStart(RE::Actor* a_actor)
{
	if (!a_actor || a_actor->IsPlayerRef() || !a_actor->IsAttacking() || a_actor->IsStaggered()) {
		return;
	}

	if (!(!a_actor->IsInKillMove() && a_actor->GetWeaponState() == RE::WEAPON_STATE::kDrawn &&
		a_actor->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal &&
		a_actor->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
		a_actor->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
		a_actor->GetFlyState() == RE::FLY_STATE::kNone)) {
		return;
	}

	if (BFCO::scar) {
		if (HasSCARComboEvent(a_actor)) {
			SKSE::log::debug("[BFCO] SCAR ativo para o NPC {:08X}. Abortando ProcessAttackWinStart.", a_actor->GetFormID());
			return;
		}
	}

	bool isComboLocked = false;
	if (a_actor->GetGraphVariableBool("BFCO_ComboLocked", isComboLocked) && isComboLocked) {
		SKSE::log::debug("[BFCO] ComboLocked = true para o NPC {:08X}. Verificando alvo...", a_actor->GetFormID());

		auto combatTarget = a_actor->GetActorRuntimeData().currentCombatTarget.get();
		if (combatTarget) {
			auto targetActor = combatTarget->As<RE::Actor>();
			if (targetActor) {
				float distance = GetPosition(a_actor).GetDistance(GetPosition(targetActor));
				float reach = a_actor->GetAttackReach();

				if (distance > (reach + 50.0f)) {
					SKSE::log::debug("[BFCO] Alvo muito longe (Dist: {}, Reach: {}). Combo cancelado.", distance, reach);
					a_actor->SetGraphVariableBool("BFCO_ComboLocked", false);
					return;
				}
				SKSE::log::debug("[BFCO] Alvo no alcance. Distancia: {}, Reach: {}", distance, reach);
			}
		}
		else {
			SKSE::log::debug("[BFCO] Nenhum combatTarget encontrado. Combo cancelado.");
			a_actor->SetGraphVariableBool("BFCO_ComboLocked", false);
			return;
		}

		// --- Motor de Decisão de Combo ---
		int nextNormal = 0;
		int nextPower = 0;
		int lastAttack = 0;

		a_actor->GetGraphVariableInt("BFCO_NextNormal", nextNormal);
		a_actor->GetGraphVariableInt("BFCO_NextPower", nextPower);
		a_actor->GetGraphVariableInt("BFCO_LastAttack", lastAttack);

		SKSE::log::debug("[BFCO] Vars do NPC: NextNormal={}, NextPower={}, LastAttack={}", nextNormal, nextPower, lastAttack);

		int highestNext = std::max(nextNormal, nextPower);

		// PREVENÇÃO DE LOOP INFINITO
		if (highestNext > 0 && highestNext <= lastAttack) {
			SKSE::log::debug("[BFCO] Loop detectado ou combo finalizado ({} <= {}). Resetando.", highestNext, lastAttack);
			a_actor->SetGraphVariableInt("BFCO_LastAttack", 0);
			a_actor->SetGraphVariableBool("BFCO_ComboLocked", false);
			a_actor->SetGraphVariableInt("BFCO_NextNormal", 0);
			a_actor->SetGraphVariableInt("BFCO_NextPower", 0);
			BFCOCombatState::ClearState(a_actor->GetFormID());
			return;
		}

		// PROBABILIDADE
		bool lastWasPower = BFCOCombatState::GetLastWasPower(a_actor->GetFormID());

		// 60% de chance se o anterior foi Power Attack, 30% se não foi.
		int powerChance = lastWasPower ? 60 : 30;
		bool preferPower = (rand() % 100 < powerChance);

		float currentStamina = a_actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);

		SKSE::log::debug("[BFCO] Ultimo ataque foi Power? {} | Sorteio Power: {} (Chance: {}%) | Stamina: {}",
			lastWasPower ? "Sim" : "Nao", preferPower ? "Sim" : "Nao", powerChance, currentStamina);

		bool executePowerAttack = false;
		int chosenAttackNum = 0;

		if (preferPower && nextPower > 0 && currentStamina > 25.0f) {
			executePowerAttack = true;
			chosenAttackNum = nextPower;
		}
		else if (nextNormal > 0 && currentStamina >= 0.0f) {
			executePowerAttack = false;
			chosenAttackNum = nextNormal;
		}
		else if (nextPower > 0 && currentStamina > 25.0f) {
			executePowerAttack = true;
			chosenAttackNum = nextPower;
		}

		SKSE::log::debug("[BFCO] Decisao Final - Executar Power: {}, Ataque Escolhido: {}", executePowerAttack ? "Sim" : "Nao", chosenAttackNum);

		if (chosenAttackNum > 0) {
			a_actor->SetGraphVariableInt("BFCO_LastAttack", chosenAttackNum);
			BFCOCombatState::SetLastWasPower(a_actor->GetFormID(), executePowerAttack);

			if (executePowerAttack) {
				if (BFCOIdles::PowerNormal && BFCOIdles::PowerNormal->conditions.IsTrue(a_actor, a_actor)) {
					SKSE::log::debug("[BFCO] Fallback para PowerNormal");
					BFCOIdles::PlayIdleAnimation(a_actor, BFCOIdles::PowerNormal);
				}
				else if (BFCOIdles::PowerH2H && BFCOIdles::PowerH2H->conditions.IsTrue(a_actor, a_actor)) {
					SKSE::log::debug("[BFCO] Condicao OK: Tocando PowerH2H");
					BFCOIdles::PlayIdleAnimation(a_actor, BFCOIdles::PowerH2H);
				}
				else if (BFCOIdles::SprintPower && BFCOIdles::SprintPower->conditions.IsTrue(a_actor, a_actor)) {
					SKSE::log::debug("[BFCO] Condicao OK: Tocando SprintPower");
					BFCOIdles::PlayIdleAnimation(a_actor, BFCOIdles::SprintPower);
				}
				
				else {
					SKSE::log::debug("[BFCO] FAILED: Nenhum Power Idle disponivel ou condicoes nao atendidas.");
				}
			}
			else {
				if (BFCOIdles::AttackNormal && BFCOIdles::AttackNormal->conditions.IsTrue(a_actor, a_actor)) {
					SKSE::log::debug("[BFCO] Tocando AttackNormal");
					BFCOIdles::PlayIdleAnimation(a_actor, BFCOIdles::AttackNormal);
				}
				else {
					SKSE::log::debug("[BFCO] FAILED: AttackNormal form nao encontrado.");
				}
			}
		}

		// Limpa a trava 
		a_actor->SetGraphVariableBool("BFCO_ComboLocked", false);
		a_actor->SetGraphVariableInt("BFCO_NextNormal", 0);
		a_actor->SetGraphVariableInt("BFCO_NextPower", 0);
	}
}

RE::BSEventNotifyControl BFCO::Hooks::NpcCycleSink::ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*)
{
	if (!a_event || !a_event->holder) return RE::BSEventNotifyControl::kContinue;

	auto* actor = a_event->holder->As<RE::Actor>();
	if (!actor || actor->IsDead()) return RE::BSEventNotifyControl::kContinue;

	const std::string_view eventName = a_event->tag;
	auto npc = const_cast<RE::Actor*>(actor);
	bool isPlayer = actor->IsPlayerRef();

	if (!BFCO::scar && !isPlayer) {
		if (eventName == "BFCO_AttackWinStart") {
			bool shouldAttack = false;
			actor->GetGraphVariableBool("BFCO_ComboLocked", shouldAttack);

			if (shouldAttack) {
				ProcessAttackWinStart(npc);
			}
		}
		else if (eventName.starts_with("BFCO_NextIsAttack")) {
			int incomingAtk = ExtractComboNumber(eventName, "BFCO_NextIsAttack");
			if (incomingAtk > 1) {
				npc->SetGraphVariableInt("BFCO_NextNormal", incomingAtk);
				npc->SetGraphVariableBool("BFCO_ComboLocked", true);
			}
		}
		else if (eventName.starts_with("BFCO_NextIsPowerAttack")) {
			int incomingPower = ExtractComboNumber(eventName, "BFCO_NextIsPowerAttack");
			if (incomingPower > 1) {
				npc->SetGraphVariableInt("BFCO_NextPower", incomingPower);
				npc->SetGraphVariableBool("BFCO_ComboLocked", true);
			}
		}
	}

	if (isPlayer) {
		
		if (eventName == "attackStartSprint" ||
			eventName == "attackPowerStart_Sprint" ||
			eventName == "attackPowerStart_2HMSprint" ||
			eventName == "attackPowerStart_2HWSprint") {
			//SKSE::log::info("[PlayerAnimationSink] >>> Gatilho DE INICIO reconhecido: {}. Executando SetSprintStaminaToZero().", eventName);
			StaminaManager::SetSprintStaminaToZero();
		}
		// Retorna ao valor padrão quando o ataque terminar
		else if (eventName == "attackStop") {
			StaminaManager::RestoreSprintStamina();
		}
	}

	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl BFCO::Hooks::NpcCombatTracker::ProcessEvent(const RE::TESCombatEvent* a_event, RE::BSTEventSource<RE::TESCombatEvent>*)
{
	if (!a_event || !a_event->actor) {
		return RE::BSEventNotifyControl::kContinue;
	}

	auto actor = a_event->actor.get();
	auto* npc = actor->As<RE::Actor>();
	if (npc && npc != RE::PlayerCharacter::GetSingleton()) { 
		switch (a_event->newState.get()) {
		case RE::ACTOR_COMBAT_STATE::kCombat:
			NpcCombatTracker::RegisterSink(npc);
			break;
		case RE::ACTOR_COMBAT_STATE::kNone:
			NpcCombatTracker::UnregisterSink(npc);
			break;
		}
	}
	return RE::BSEventNotifyControl::kContinue;
}

void BFCO::Hooks::NpcCombatTracker::RegisterSink(RE::Actor* a_actor)
{
	std::unique_lock lock(g_mutex);
	if (g_trackedNPCs.find(a_actor->GetFormID()) == g_trackedNPCs.end()) {
		a_actor->AddAnimationGraphEventSink(&g_npcSink);
		g_trackedNPCs.insert(a_actor->GetFormID());
	}
}

void BFCO::Hooks::NpcCombatTracker::UnregisterSink(RE::Actor* a_actor)
{
	if (!a_actor || a_actor->IsPlayerRef()) return;

	std::unique_lock lock(g_mutex);
	if (g_trackedNPCs.find(a_actor->GetFormID()) != g_trackedNPCs.end()) {
		a_actor->RemoveAnimationGraphEventSink(&g_npcSink);
		g_trackedNPCs.erase(a_actor->GetFormID());
	}
}

void BFCO::Hooks::NpcCombatTracker::RegisterSinksForExistingCombatants()
{
	auto* processLists = RE::ProcessLists::GetSingleton();
	if (!processLists) {
		SKSE::log::warn("[NpcCombatTracker] Não foi possível obter ProcessLists.");
		return;
	}

	// Itera sobre todos os atores que estão "ativos" no jogo
	for (auto& actorHandle : processLists->highActorHandles) {
		if (auto actor = actorHandle.get().get()) {
			// A função IsInCombat() nos diz se o ator já está em um estado de combate
			if (!actor->IsPlayerRef()) {
				if (actor->IsInCombat()) {
					SKSE::log::info("[NpcCombatTracker] Ator '{}' ({:08X}) já está em combate. Registrando sink...",
						actor->GetName(), actor->GetFormID());
					// Usamos a mesma função de registro que já existe!
					RegisterSink(actor);
				}
			}

		}
	}

	SKSE::log::info("[NpcCombatTracker] Verificação concluída.");
}


inline std::array blockedMenus = {
	RE::DialogueMenu::MENU_NAME,    RE::JournalMenu::MENU_NAME,    RE::MapMenu::MENU_NAME,
	RE::StatsMenu::MENU_NAME,       RE::ContainerMenu::MENU_NAME,  RE::InventoryMenu::MENU_NAME,
	RE::TweenMenu::MENU_NAME,       RE::TrainingMenu::MENU_NAME,   RE::TutorialMenu::MENU_NAME,
	RE::LockpickingMenu::MENU_NAME, RE::SleepWaitMenu::MENU_NAME,  RE::LevelUpMenu::MENU_NAME,
	RE::Console::MENU_NAME,         RE::BookMenu::MENU_NAME,       RE::CreditsMenu::MENU_NAME,
	RE::LoadingMenu::MENU_NAME,     RE::MessageBoxMenu::MENU_NAME, RE::MainMenu::MENU_NAME,
	RE::RaceSexMenu::MENU_NAME,     RE::FavoritesMenu::MENU_NAME
	//,  std::string_view("LootMenu"),std::string_view("LootMenuIE") 
};

bool IsAnyMenuOpen() {
	const auto ui = RE::UI::GetSingleton();
	for (const auto a_name : blockedMenus) {
		if (ui->IsMenuOpen(a_name)) {
			return true;
		}
	}
	return false;
}

void AttackStateManager::Register() {
	auto input = RE::BSInputDeviceManager::GetSingleton();
	if (input) {
		input->AddEventSink(this);
		SKSE::log::info("SUCESSO: Listener de eventos de input registrado.");
	}
	else {
		SKSE::log::error(
			"FALHA: O gerenciador de input (BSInputDeviceManager) é nulo. O listener não pôde ser registrado.");
	}
}

RE::BSEventNotifyControl AttackStateManager::ProcessEvent(RE::InputEvent* const* a_event,
	RE::BSTEventSource<RE::InputEvent*>* a_source) {
	if (!a_event || !*a_event) {
		return RE::BSEventNotifyControl::kContinue;
	}

	auto player = RE::PlayerCharacter::GetSingleton();
	if (!player || !player->Is3DLoaded()) {
		return RE::BSEventNotifyControl::kContinue;
	}

	for (auto event = *a_event; event; event = event->next) {
		if (event->eventType != RE::INPUT_EVENT_TYPE::kButton) {
			continue;
		}

		auto buttonEvent = event->AsButtonEvent();
		if (!buttonEvent) {
			continue;
		}

		if (IsAnyMenuOpen()) {
			return RE::BSEventNotifyControl::kContinue;
		}

		auto device = buttonEvent->GetDevice();
		auto rawKeyCode = buttonEvent->GetIDCode();
		auto keyCode = rawKeyCode;
		const auto playerState = player->AsActorState();


		if (!(!player->IsInKillMove() && playerState->GetWeaponState() == RE::WEAPON_STATE::kDrawn &&
			playerState->GetSitSleepState() == RE::SIT_SLEEP_STATE::kNormal &&
			playerState->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
			playerState->GetKnockState() == RE::KNOCK_STATE_ENUM::kNormal &&
			playerState->GetFlyState() == RE::FLY_STATE::kNone)) {
			return RE::BSEventNotifyControl::kContinue;
		}

		if (device == RE::INPUT_DEVICE::kMouse) {
			keyCode += 256;
		}
		else if (device == RE::INPUT_DEVICE::kGamepad) {

		}

		bool isPowerAttackKeyPressed = (Settings::iKeyAttackPowerNUM != -1 && keyCode == Settings::iKeyAttackPowerNUM);
		bool comboKeyPressed = (Settings::bKeyAttackComb && Settings::iKeyAttackComb != -1 && keyCode == Settings::iKeyAttackComb);

		if (buttonEvent->IsDown()) {
			if (isPowerAttackKeyPressed) {
				player->NotifyAnimationGraph("MCO_EndAnimation");

				auto playDirectionalPowerAttack = [](RE::Actor* p) {
					if (BFCOIdles::PowerDirA && BFCOIdles::PowerDirA->conditions.IsTrue(p, p)) {
						BFCOIdles::PlayIdleAnimation(p, BFCOIdles::PowerDirA);
					}
					else if (BFCOIdles::PowerDirB && BFCOIdles::PowerDirB->conditions.IsTrue(p, p)) {
						BFCOIdles::PlayIdleAnimation(p, BFCOIdles::PowerDirB);
					}
					else if (BFCOIdles::PowerDirL && BFCOIdles::PowerDirL->conditions.IsTrue(p, p)) {
						BFCOIdles::PlayIdleAnimation(p, BFCOIdles::PowerDirL);
					}
					else if (BFCOIdles::PowerDirR && BFCOIdles::PowerDirR->conditions.IsTrue(p, p)) {
						BFCOIdles::PlayIdleAnimation(p, BFCOIdles::PowerDirR);
					}
					else if (BFCOIdles::PowerNormal && BFCOIdles::PowerNormal->conditions.IsTrue(p, p)) {
						BFCOIdles::PlayIdleAnimation(p, BFCOIdles::PowerNormal);
					}
					else {
						BFCOIdles::PerformAction(BFCOIdles::PowerRight, p);
					}
				};


				if (BFCOIdles::JumpPower && BFCOIdles::JumpPower->conditions.IsTrue(player, player)) {
					player->NotifyAnimationGraph("BfcoJumpStop");

					auto playerHandle = player->GetHandle();
					std::thread([playerHandle]() {
						std::this_thread::sleep_for(std::chrono::milliseconds(50));
						SKSE::GetTaskInterface()->AddTask([playerHandle]() {
							if (auto p = playerHandle.get()) {
								BFCOIdles::PlayIdleAnimation(p.get(), BFCOIdles::JumpPower);
							}
							});
						}).detach();

				}
				else if (BFCOIdles::SprintPower && BFCOIdles::SprintPower->conditions.IsTrue(player, player)) {
					BFCOIdles::PlayIdleAnimation(player, BFCOIdles::SprintPower);

				}
				else if (BFCOIdles::PowerBash && BFCOIdles::PowerBash->conditions.IsTrue(player, player)) {
					BFCOIdles::PlayIdleAnimation(player, BFCOIdles::PowerBash);

				}
				else {
					if (BFCOIdles::CancelDodge && BFCOIdles::CancelDodge->conditions.IsTrue(player, player)) {
						BFCOIdles::PlayIdleAnimation(player, BFCOIdles::CancelDodge);

						auto playerHandle = player->GetHandle();
						std::thread([playerHandle, playDirectionalPowerAttack]() {
							std::this_thread::sleep_for(std::chrono::milliseconds(50)); 
							SKSE::GetTaskInterface()->AddTask([playerHandle, playDirectionalPowerAttack]() {
								if (auto p = playerHandle.get()) {
									playDirectionalPowerAttack(p.get());
								}
								});
							}).detach();
					}
					else {
						playDirectionalPowerAttack(player);
					}
				}
			}

			if (comboKeyPressed) {
				if (BFCOIdles::SprintPower->conditions.IsTrue(player, player)) {
					player->NotifyAnimationGraph("MCO_EndAnimation");
					BFCOIdles::PlayIdleAnimation(player, BFCOIdles::SprintPower);
				}
				else if (BFCOIdles::PowerBash->conditions.IsTrue(player, player)) {
					player->NotifyAnimationGraph("MCO_EndAnimation");
					BFCOIdles::PlayIdleAnimation(player, BFCOIdles::PowerBash);
				}
				else {
					player->NotifyAnimationGraph("MCO_EndAnimation");
					player->NotifyAnimationGraph("BFCOAttackStart_Comb");
					BFCOIdles::PlayIdleAnimation(player, BFCOIdles::ComboAttack); 
				}
			}
		}

		if (buttonEvent->IsUp()) {
			if (isPowerAttackKeyPressed) {
				player->NotifyAnimationGraph("BFCOAttackstart_1");
			}
		}
	}
	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl MenuWatcher::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{

	if (a_event && !a_event->opening && a_event->menuName == RE::JournalMenu::MENU_NAME) {
			Settings::Load();
	}

	return RE::BSEventNotifyControl::kContinue;
}


void StaminaManager::SetSprintStaminaToZero() {
	//SKSE::log::info("[StaminaManager] [SET] Tentando zerar o dreno de estamina do sprint...");

	auto gmst = RE::GameSettingCollection::GetSingleton();
	auto drainSetting = gmst ? gmst->GetSetting("fSprintStaminaDrainMult") : nullptr;

	// Fallback para buscar pelo FormID caso não encontre pela string
	if (!drainSetting) {
		SKSE::log::warn("[StaminaManager] [SET] Falha ao achar a GameSetting por string. Tentando pelo FormID 0002DD35...");
		auto form = RE::TESForm::LookupByID(0x0002DD35);
		if (form) {
			//SKSE::log::info("[StaminaManager] [SET] FormID 0002DD35 encontrado! (FormType: {}). Convertendo...", (int)form->GetFormType());
		}
	}

	if (!drainSetting) {
		SKSE::log::error("[StaminaManager] [SET] ERRO: Nao foi possivel localizar a setting fSprintStaminaDrainMult na memoria!");
		return;
	}

	// 1. Salva o original se ainda não salvou
	if (defaultSprintStaminaDrain < 0.0f) {
		defaultSprintStaminaDrain = drainSetting->GetFloat();
		//SKSE::log::info("[StaminaManager] [SET] Valor padrao de estamina salvo no cache: {}", defaultSprintStaminaDrain);
	}

	// 2. Zera o custo
	drainSetting->data.f = 0.0f;
	//SKSE::log::info("[StaminaManager] [SET] Dreno de sprint alterado com sucesso para: {}", drainSetting->data.f);
}

void StaminaManager::RestoreSprintStamina() {
	//SKSE::log::info("[StaminaManager] [RESTORE] Tentando restaurar o dreno de estamina do sprint...");

	auto gmst = RE::GameSettingCollection::GetSingleton();
	auto drainSetting = gmst ? gmst->GetSetting("fSprintStaminaDrainMult") : nullptr;

	if (!drainSetting) {
		SKSE::log::error("[StaminaManager] [RESTORE] ERRO: Nao achou o GameSetting para restaurar!");
		return;
	}

	// 3. Restaura o original se estava salvo
	if (defaultSprintStaminaDrain >= 0.0f) {
		drainSetting->data.f = defaultSprintStaminaDrain;
		//SKSE::log::info("[StaminaManager] [RESTORE] Valor restaurado para o original: {}", defaultSprintStaminaDrain);
		defaultSprintStaminaDrain = -1.0f; // Reseta o cache
	}
	else {
		//SKSE::log::info("[StaminaManager] [RESTORE] Ignorado. O valor original não estava salvo (cache = {}).", defaultSprintStaminaDrain);
	}
}

void BFCO::Hooks::ScheduleSinkRegistration(RE::Actor* actor, int attempts)
{
	if (attempts > 20) {
		SKSE::log::critical("[Actor3DLoadEventHandler] Desistindo após {} tentativas para o ator {:08X}.", attempts, actor->GetFormID());
		return;
	}

	std::thread([actor, attempts]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		SKSE::GetTaskInterface()->AddTask([actor, attempts]() {
			if (!actor) return;
			auto ui = RE::UI::GetSingleton();
			if (ui && (ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME))) {
				SKSE::log::info("[Actor3DLoadEventHandler] Operação cancelada. Jogo no Main Menu ou Carregando (Ator {:08X}).", actor->GetFormID());
				return;
			}
			RE::BSTSmartPointer<RE::BSAnimationGraphManager> graphManager;
			actor->GetAnimationGraphManager(graphManager);

			if (graphManager) {
				//SKSE::log::info("[Actor3DLoadEventHandler] Graph encontrado para {:08X}. Reconectando...", actor->GetFormID());

				if (actor->IsPlayerRef()) {
					actor->RemoveAnimationGraphEventSink(BFCO::Hooks::NpcCycleSink::GetSingleton());
					if (actor->AddAnimationGraphEventSink(BFCO::Hooks::NpcCycleSink::GetSingleton())) {
						SKSE::log::info("[Actor3DLoadEventHandler] Sink do PLAYER reconectada com sucesso.");
					}

				}
				else {

					BFCO::Hooks::NpcCombatTracker::UnregisterSink(actor);
					BFCO::Hooks::NpcCombatTracker::RegisterSink(actor);

					SKSE::log::info("[Actor3DLoadEventHandler] Sink de NPC reconectada (via CombatTracker).");
				}
			}
			else {
				ScheduleSinkRegistration(actor, attempts + 1);
			}
			});
		}).detach();
}

RE::BSEventNotifyControl BFCO::Hooks::PC3DLoadEventHandler::ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*)
{
	if (!a_event || !a_event->loaded) {
		return RE::BSEventNotifyControl::kContinue;
	}

	// Em vez de pegar o Player Singleton, buscamos o formulário pelo ID do evento
	auto* form = RE::TESForm::LookupByID(a_event->formID);
	if (!form) return RE::BSEventNotifyControl::kContinue;

	// Tentamos converter para Ator. Se não for ator (ex: uma parede), ignoramos.
	auto* actor = form->As<RE::Actor>();

	if (actor) {
		ScheduleSinkRegistration(actor, 0);
	}

	return RE::BSEventNotifyControl::kContinue;
}
