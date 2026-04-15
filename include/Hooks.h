
static void Install()
{
	//REL::Relocation<std::uintptr_t> vtable{ RE::Character::VTABLE[2] };
	//_ProcessEvent = vtable.write_vfunc(0x1, &Hook_ProcessEvent);
	//stl::write_vfunc<0x1, AnimEventHook>(REL::VariantID(261399, 207890, 0x16d7760));
}

//static RE::BSEventNotifyControl Hook_ProcessEvent(
//	RE::BSTEventSink<RE::BSAnimationGraphEvent>* a_this,
//	RE::BSAnimationGraphEvent* a_event,
//	RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_dispatcher)
//{
//	if (a_event && a_event->holder) {
//		std::string_view tag = a_event->tag.c_str();
//		auto actor = const_cast<RE::Actor*>(a_event->holder->As<RE::Actor>());
//		auto singleton = BFCO::GetSingleton();
//		if (actor) {
//			if (tag == "BFCO_AttackWinStart") {
//				int currentAtkValue = 0;
//				bool shouldAttack = false;
//				actor->GetGraphVariableBool("BFCO_ComboLocked", shouldAttack);
//				// CORREÇÃO: Passamos a variável por referência, sem atribuir o retorno bool a ela
//				actor->GetGraphVariableInt("BFCO_LastAttack", currentAtkValue);

//				logger::debug("[ATTACK] Valor real do LastAttack: {} para {}", currentAtkValue, actor->GetName());

//				if (currentAtkValue > 1 && shouldAttack) {
//					singleton->ProcessAttackWinStart(actor);
//				}
//			}
//			else if (tag.starts_with("BFCO_NextIsAttack")) {
//				int incomingAtk = singleton->ExtractComboNumber(tag);
//				int currentAtk = 0;
//				actor->GetGraphVariableInt("BFCO_LastAttack", currentAtk);

//				if (incomingAtk < currentAtk) {
//					// Se o novo ataque é menor que o anterior, o combo resetou/terminou
//					actor->SetGraphVariableInt("BFCO_LastAttack", 0);
//					actor->SetGraphVariableBool("BFCO_ComboLocked", false);
//					logger::debug("[COMBO] Resetado: {} < {} para {}", incomingAtk, currentAtk, actor->GetName());
//					return _ProcessEvent(a_this, a_event, a_dispatcher);
//				}
//				else if (incomingAtk > 1)
//				{
//					// Se for maior ou igual, o combo progride
//					actor->SetGraphVariableInt("BFCO_LastAttack", incomingAtk);
//					actor->SetGraphVariableBool("BFCO_ComboLocked", true);
//					logger::debug("[COMBO] Progressão: {} para {}", incomingAtk, actor->GetName());

//				}
//					

//			}
//		}
//		

//	}
//	return _ProcessEvent(a_this, a_event, a_dispatcher);
//}

/*static inline REL::Relocation<decltype(Hook_ProcessEvent)> _ProcessEvent;*/