#include "common.hpp"

namespace intercept::assembly {

	asshelper::asshelper(game_state* gs)
	{
		nmap["missionnamespace"] = sqf::mission_namespace();
		nmap["uinamespace"] = sqf::ui_namespace();
		nmap["parsingnamespace"] = sqf::parsing_namespace();
		nmap["profilenamespace"] = sqf::profile_namespace();

		umap["sqrt"] = [](game_value right, game_value *out) -> int {
			if (right.type_enum() != types::GameDataType::SCALAR) { return 0; }
			*out = game_value(sqrt((float)right));
			return 1;
		};
		bmap["mod"] = [](game_value left, game_value right, game_value *out) -> int {
			if (left.type_enum() != types::GameDataType::SCALAR || right.type_enum() != types::GameDataType::SCALAR) { return 0; }
			*out = game_value(fmodf((float)left, (float)right));
			return 2;
		};
	}
	bool asshelper::containsNular(const char* key) const { return nmap.find(key) != nmap.end(); }
	bool asshelper::containsUnary(const char* key) const { return umap.find(key) != umap.end(); }
	bool asshelper::containsBinary(const char* key) const { return bmap.find(key) != bmap.end(); }

	game_value asshelper::get(const char* key) const { return nmap.at(key); }
	int asshelper::get(game_state* gs, const char* key, ref<game_instruction> right, game_value *out) const
	{
		if (!isconst(gs, this, right))
			return 0;
		auto rightval = static_cast<GameInstructionConst*>(right.get());
		return umap.at(key)(rightval, out);
	}
	int asshelper::get(game_state* gs, const char* key, ref<game_instruction> left, ref<game_instruction> right, game_value *out) const
	{
		if (!isconst(gs, this, left) || !isconst(gs, this, right))
			return 0;
		auto leftval = static_cast<GameInstructionConst*>(left.get());
		auto rightval = static_cast<GameInstructionConst*>(right.get());
		return bmap.at(key)(leftval, rightval, out);
	}

	enum insttype
	{
		NA = -1,
		endStatement,
		push,
		callUnary,
		callBinary,
		assignToLocal,
		assignTo,
		callNular,
		getVariable,
		makeArray
	};
	insttype getinsttype(game_state* gs, ref<game_instruction> instr)
	{
		auto typeHash = typeid(*instr.get()).hash_code();
		std::string typeName = typeid(*instr.get()).name();

		switch (typeHash) {
		case GameInstructionNewExpression::typeIDHash:
			return insttype::endStatement;
		case GameInstructionConst::typeIDHash:
			return insttype::push;
		case GameInstructionFunction::typeIDHash: {
			return insttype::callUnary;
		} break;
		case GameInstructionOperator::typeIDHash:
			return insttype::callBinary;
		case GameInstructionAssignment::typeIDHash: {
			GameInstructionAssignment* inst = static_cast<GameInstructionAssignment*>(instr.get());
			if (inst->forceLocal) {
				return insttype::assignToLocal;
			}
			else {
				return insttype::assignTo;
			}
		} break;
		case GameInstructionVariable::typeIDHash: {
			GameInstructionVariable* inst = static_cast<GameInstructionVariable*>(instr.get());
			auto varname = inst->name;
			if (gs->_scriptNulars.has_key(varname.c_str())) {
				return insttype::callNular;
			}
			else {
				return insttype::getVariable;
			}
		} break;
		case GameInstructionArray::typeIDHash:
			return insttype::makeArray;
		default:
			return insttype::NA;
		}
	}

	bool isconst(game_state* gs, const asshelper* nh, ref<game_instruction> instr)
	{
		auto type = getinsttype(gs, instr);
		if (type == insttype::push)
		{
			return true;
		}
		else if (type == insttype::callNular)
		{
			auto inst = static_cast<GameInstructionVariable*>(instr.get());
			if (nh->containsNular(inst->name.c_str()))
			{
				return true;
			}
		}
		return false;
	}
	game_value getconst(game_state* gs, asshelper* nh, ref<game_instruction> instr)
	{
		auto type = getinsttype(gs, instr);
		if (type == insttype::push)
		{
			return static_cast<GameInstructionConst*>(instr.get())->value;
		}
		else if (type == insttype::callNular)
		{
			auto inst = static_cast<GameInstructionVariable*>(instr.get());
			return nh->get(inst->name.c_str());
		}
		else
		{
			throw std::exception();
		}
	}
	void optimize(game_state* gs, asshelper* nh, ref<compact_array<ref<game_instruction>>> instructions)
	{
		size_t count = instructions->size();
		int died = 0;
		for (int i = 0; i < count; i++)
		{
			auto instr = instructions->get(i);
			//GameInstructionConst::make(array);
			switch (getinsttype(gs, instr))
			{
				case insttype::makeArray: {
					auto inst = static_cast<GameInstructionArray*>(instr.get());
					size_t arrsize = inst->size;
					//In case makeArray has zero size, just transform to a push instruction
					if (arrsize == 0)
					{
						instructions->data()[i] = GameInstructionConst::make(auto_array<game_value>());
						break;
					}
					bool abortflag = false;
					//Backtrack - Check if non-constant values are existing
					for (int j = i - arrsize - died; j < i - died; j++)
					{
						if (!isconst(gs, nh, instructions->get(j)))
						{
							abortflag = true;
							break;
						}
					}
					//If abortflag was set, abort conversion
					if (abortflag)
					{
						break;
					}
					//Backtrack - Add elements to array
					auto_array<game_value> arr;
					for (int j = i - arrsize - died; j < i - died; j++)
					{
						arr.push_back(getconst(gs, nh, instructions->get(j)));
					}
					died += arrsize;
					instructions->data()[i] = GameInstructionConst::make(std::move(arr));
				} break;
				case insttype::callUnary: {
					auto inst = static_cast<GameInstructionFunction*>(instr.get());
					game_value valueslot;
					int diedslot = nh->get(gs, inst->getFuncName().c_str(), instructions->get(i - died - 1), &valueslot);
					if (diedslot)
					{
						died += diedslot;
						instructions->data()[i] = GameInstructionConst::make(valueslot);
					}
				} break;
				case insttype::callBinary: {
					auto inst = static_cast<GameInstructionFunction*>(instr.get());
					game_value valueslot;
					int diedslot = nh->get(gs, inst->getFuncName().c_str(), instructions->get(i - died - 2), instructions->get(i - died - 1), &valueslot);
					if (diedslot)
					{
						died += diedslot;
						instructions->data()[i] = GameInstructionConst::make(valueslot);
					}
				} break;
			}
			if (died > 0)
			{
				instructions->data()[i - died] = instructions->data()[i];
			}
		}
		for (auto i = instructions->begin() + (count - died); i < instructions->end(); i++)
		{
			i->free();
		}
		instructions->_size -= died;
	}
}