#pragma once
#include <shared_mutex>

namespace BFCOIdles {
	inline RE::TESIdleForm* AttackNormal = nullptr;
	inline RE::TESIdleForm* PowerNormal = nullptr;
	inline RE::TESIdleForm* PowerH2H = nullptr;
	inline RE::TESIdleForm* PowerBash = nullptr;
	inline RE::TESIdleForm* SprintPower = nullptr;
	inline RE::TESIdleForm* ComboAttack = nullptr;
	inline RE::TESIdleForm* JumpPower = nullptr;
	inline RE::TESIdleForm* CancelDodge = nullptr;
	inline RE::TESIdleForm* BlockStart = nullptr;
	inline RE::TESIdleForm* PowerDirA = nullptr;
	inline RE::TESIdleForm* PowerDirB = nullptr;
	inline RE::TESIdleForm* PowerDirL = nullptr;
	inline RE::TESIdleForm* PowerDirR = nullptr;

	inline RE::BGSAction* PowerRight = nullptr;
	inline void PerformAction(RE::BGSAction* action, RE::Actor* player);

	inline RE::TESIdleForm* GetIdleByFormID(RE::FormID formID, const std::string& pluginName);
	void InitIdles();
	inline void PlayIdleAnimation(RE::Actor* actor, RE::TESIdleForm* idle);
}

namespace Settings {
	inline bool bKeyAttackComb = true;
	inline int32_t iKeyAttackComb = -1;
	inline int32_t iKeyAttackPowerNUM = -1;

	inline void Load() {
		auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			logger::error("Não foi possível acessar o TESDataHandler para carregar as configurações.");
			return;
		}

		const std::string_view pluginName = "SCSI-ACTbfco-Main.esp";


		auto globalAttackCombBool = dataHandler->LookupForm<RE::TESGlobal>(0x84A, pluginName);
		auto globalAttackComb = dataHandler->LookupForm<RE::TESGlobal>(0x84B, pluginName);
		auto globalAttackPower = dataHandler->LookupForm<RE::TESGlobal>(0x84D, pluginName);


		if (globalAttackCombBool) {
			bKeyAttackComb = static_cast<int>(globalAttackCombBool->value) != 0;
		}
		else {
			logger::warn("Global Variable 0x84A (bKeyAttackComb) não encontrada.");
		}

		if (globalAttackComb) {
			iKeyAttackComb = static_cast<int32_t>(globalAttackComb->value);
		}
		else {
			logger::warn("Global Variable 0x84B (iKeyAttackComb) não encontrada.");
		}

		if (globalAttackPower) {
			iKeyAttackPowerNUM = static_cast<int32_t>(globalAttackPower->value);
		}
		else {
			logger::warn("Global Variable 0x84D (iKeyAttackPowerNUM) não encontrada.");
		}

		logger::info("MCM Settings Loaded from Globals: bKeyAttackComb={}, iKeyAttackComb={}, iKeyAttackPowerNUM={}",
			bKeyAttackComb, iKeyAttackComb, iKeyAttackPowerNUM);
	}
}

class BFCO
{
public:
	static BFCO* GetSingleton()
	{
		static BFCO singleton;
		return &singleton;
	}
	static inline bool scar = false;
	static void InstallHooks()
	{
		Hooks::Install();
	}

	enum class Direction
	{
		kNeutral = 0,
		kForward,
		kStrafeRight,
		kBack,
		kStrafeLeft,
	};

	enum class DirectionOcto
	{
		kNeutral = 0,
		kForward,
		kForwardRight,
		kStrafeRight,
		kBackRight,
		kBack,
		kBackLeft,
		kStrafeLeft,
		kForwardLeft
	};



	Direction GetDirection(RE::NiPoint2 a_vec, bool a_gamepad);
	DirectionOcto GetDirectionOcto(RE::NiPoint2 a_vec, bool a_gamepad);

	void ProcessMovement(RE::PlayerControlsData* a_data, bool a_gamepad);


	struct Hooks
	{
		struct MovementHandler_ProcessThumbstick
		{
			static void thunk(RE::MovementHandler* a_this, RE::ThumbstickEvent* a_event, RE::PlayerControlsData* a_data)
			{
				func(a_this, a_event, a_data);
				GetSingleton()->ProcessMovement(a_data, true);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct MovementHandler_ProcessButton
		{
			static void thunk(RE::MovementHandler* a_this, RE::ButtonEvent* a_event, RE::PlayerControlsData* a_data)
			{
				func(a_this, a_event, a_data);
				GetSingleton()->ProcessMovement(a_data, false);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		static void Install()
		{
			stl::write_vfunc<0x2, MovementHandler_ProcessThumbstick>(RE::VTABLE_MovementHandler[0]);
			stl::write_vfunc<0x4, MovementHandler_ProcessButton>(RE::VTABLE_MovementHandler[0]);
		}

		static void ScheduleSinkRegistration(RE::Actor* actor, int attempts);


		class NpcCycleSink : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
		public:
			static NpcCycleSink* GetSingleton() {
				static NpcCycleSink singleton;
				return &singleton;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
				RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override;
		};



		class NpcCombatTracker : public RE::BSTEventSink<RE::TESCombatEvent> {
		public:
			static NpcCombatTracker* GetSingleton() {
				static NpcCombatTracker singleton;
				return &singleton;
			}

			// Função chamada quando um evento de combate ocorre
			RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* a_event,
				RE::BSTEventSource<RE::TESCombatEvent>*) override;

			static void RegisterSink(RE::Actor* a_actor);
			static void UnregisterSink(RE::Actor* a_actor);

			static void RegisterSinksForExistingCombatants();

		private:
			// Instância compartilhada do nosso processador de lógica
			inline static NpcCycleSink g_npcSink;

			// Guarda os FormIDs dos NPCs que já estamos ouvindo
			inline static std::set<RE::FormID> g_trackedNPCs;
			inline static std::shared_mutex g_mutex;
		};

		class PC3DLoadEventHandler : public RE::BSTEventSink<RE::TESObjectLoadedEvent> {
		public:
			static PC3DLoadEventHandler* GetSingleton() {
				static PC3DLoadEventHandler singleton;
				return &singleton;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override;
		};

	};

private:
	BFCO() = default;

};

class AttackStateManager
	: public RE::BSTEventSink<RE::InputEvent*> {  
public:

	static AttackStateManager* GetSingleton() {
		static AttackStateManager singleton;
		return &singleton;
	}
	void Register();

	RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,
		RE::BSTEventSource<RE::InputEvent*>* a_source) override;


private:


};


class MenuWatcher : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	static MenuWatcher* GetSingleton()
	{
		static MenuWatcher singleton;
		return &singleton;
	}

	void Register()
	{
		auto ui = RE::UI::GetSingleton();
		if (ui) {
			ui->AddEventSink(this);
			SKSE::log::info("MenuWatcher registrado para monitorar o JournalMenu.");
		}
	}

	RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;
};

namespace StaminaManager {
	inline float defaultSprintStaminaDrain = -1.0f;
	void SetSprintStaminaToZero();
	void RestoreSprintStamina();
}
