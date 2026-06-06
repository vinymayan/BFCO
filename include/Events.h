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
	inline RE::TESIdleForm* PowerDirA3rd = nullptr;
	inline RE::TESIdleForm* PowerDirB3rd = nullptr;
	inline RE::TESIdleForm* PowerDirL3rd = nullptr;
	inline RE::TESIdleForm* PowerDirR3rd = nullptr;
	inline RE::TESIdleForm* SpecialAttack = nullptr;
	inline RE::TESIdleForm* PowerSpecialAttack = nullptr;

	inline RE::TESIdleForm* PowerRight = nullptr;
	inline void PerformAction(RE::BGSAction* action, RE::Actor* player);

	inline RE::TESIdleForm* GetIdleByFormID(RE::FormID formID, const std::string& pluginName);
	void InitIdles();
	inline void PlayIdleAnimation(RE::Actor* actor, RE::TESIdleForm* idle);
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
	static inline bool CMF = false;
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
	: public RE::BSTEventSink<SKSE::ModCallbackEvent> { 
public:

	static AttackStateManager* GetSingleton() {
		static AttackStateManager singleton;
		return &singleton;
	}
	void Register();

	RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*) override;
	bool isComboActive = false;
private:

};

namespace StaminaManager {
	inline float defaultSprintStaminaDrain = -1.0f;
	void SetSprintStaminaToZero();
	void RestoreSprintStamina();
}
