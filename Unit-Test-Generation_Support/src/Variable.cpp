#include <string>
#include <iostream>
#include "Variabile.h"
#include "Unit-Test-Generation-Support.h"

std::string Variable::print() {
	std::string prefix = "";
	std::string postfix = "";
	this->print(prefix, postfix);
	return "";
}

std:: string Variable::print(std::string prefix, std::string postfix) {
	std::string tempPrefix;
	std::string tempPostfix;
	std::string res;
	bool unsignedType = false;
	if (std::regex_search(this->type, std::regex("(.*)unsigned"))) {
		unsignedType = true;
	}
	if (this->uData.uSize.getBytes() == 4) {
		if (unsignedType) {
			res = prefix + this->name + postfix + " = " + std::to_string((uint32_t)this->value) + "\n";
		}
		else {
			res = prefix + this->name + postfix + " = " + std::to_string((int32_t)this->value) + "\n";
		}
	}
	else if (this->uData.uSize.getBytes() == 2) {
		if (unsignedType) {
			res = prefix + this->name + postfix + " = " + std::to_string((uint16_t)this->value) + "\n";
		}
		else {
			res = prefix + this->name + postfix + " = " + std::to_string((int16_t)this->value) + "\n";
		}
	}
	else if (this->uData.uSize.getBytes() == 1) {
		if (unsignedType) {
			res = prefix + this->name + postfix + " = " + std::to_string((uint8_t)this->value) + "\n";
		}
		else {
			res = prefix + this->name + postfix + " = " + std::to_string((int8_t)this->value) + "\n";
		}
	}
	else {
		res = prefix + this->name + postfix + " = " + std::to_string((uint32_t)this->value) + "\n";
	}
	std::cout << res;
	Variable::outputFile << res;
	int ix = 0;
	if (this->typeEnum == Variable::typeEnum_t::isArray) {
		for (auto v = this->array.begin(); v != this->array.end(); v++) {
			tempPrefix = prefix + this->name + "[" + std::to_string(ix) + "]";
			tempPostfix = postfix;
			v->print(tempPrefix, tempPostfix);
			ix++;
		}
	}
	if (this->typeEnum == Variable::typeEnum_t::isStruct) {
		for (auto v = this->intStruct.begin(); v != this->intStruct.end(); v++) {
			tempPrefix = prefix + this->name + ".";
			tempPostfix = postfix;
			v->print(tempPrefix, tempPostfix);
		}
	}
	if (this->typeEnum == Variable::typeEnum_t::isUnion) {
		for (auto v = this->intStruct.begin(); v != this->intStruct.end(); v++) {
			tempPrefix = prefix + this->name + ".";
			tempPostfix = postfix;
			v->print(tempPrefix, tempPostfix);
		}
	}
	if (this->typeEnum == Variable::typeEnum_t::isRef) {
		if (this->pointsTo) {
			tempPrefix = prefix + this->name + "->";
			tempPostfix = postfix;
			this->pointsTo->print(tempPrefix, postfix);
		}
		else {
			std::cout << "Null pointer\n";
			Variable::outputFile << "Null pointer\n";
		}
	}
	prefix = "";
	return prefix;
}

void Variable::updateCommonArea(void) {
	if (this->uData.myParent && 
		this->uData.myParent->typeEnum == Variable::typeEnum_t::isUnion) 
	{
		int bbOffset_byte = this->uData.uOffset.getBytes();
		int bbOffset_bit = this->uData.uOffset.getBits();
		int bbSize_byte = this->uData.uSize.getBytes();
		int bbSize_bit = this->uData.uSize.getBits();
		/* In case that we have to copy Bytes (not Bits)
		*/
		if (bbSize_bit) {
			/* Copy verbatim bit by bit */
			int length = bbSize_byte * 8 + bbSize_bit;
			BbSize from;
			BbSize to =  this->uData.uOffset;
			BbSize inc = BbSize(1, 0);
			bool val;
			unsigned char mask8;
			for (int ix = 0; ix < length; ix++) {
				val = ((unsigned char*)&this->value)[from.getBytes()] & (1 << from.getBits());
				mask8 = 1 << to.getBits();
				this->uData.myParent->intUnion.at(to.getBytes()) &= (~mask8);
				if (val) {
					this->uData.myParent->intUnion.at(to.getBytes()) |= mask8;
				}
				from = from + inc;
				to = to + inc;
			}
		}
		else {
			for (int i = 0; i < bbSize_byte; i++) {
				this->uData.myParent->intUnion.at(bbOffset_byte++) =
					((unsigned char*)&this->value)[i];
			}
		}
		/* Now update all the members */
		this->uData.myParent->updateUnion();
	}
}

void Variable::updateUnion() {
	if (this->typeEnum == Variable::typeEnum_t::isArray) {
		for (auto it = this->array.begin(); it != this->array.end(); it++) {
			(*it).updateUnion();
		}
	}
	else if (this->typeEnum == Variable::typeEnum_t::isStruct) {
		for (auto it = this->intStruct.begin(); it != this->intStruct.end(); it++) {
			(*it).updateUnion();
		}
	}
	else if (this->typeEnum == Variable::typeEnum_t::isUnion) {
		for (auto it = this->intStruct.begin(); it != this->intStruct.end(); it++) {
			(*it).updateUnion();
		}
	}
	else {
		Variable* p; // parent
		unsigned long long value = 0;
		p = this->uData.myParent;
		int bbOffset_byte = this->uData.uOffset.getBytes();
		int bbOffset_bit = this->uData.uOffset.getBits();
		int bbSize_byte = this->uData.uSize.getBytes();
		int bbSize_bit = this->uData.uSize.getBits();
		if (bbSize_bit) {
			/* Copy verbatim bit by bit */
			int length = bbSize_byte * 8 + bbSize_bit;
			BbSize from = this->uData.uOffset;
			BbSize to;
			BbSize inc = BbSize(0,1);
			bool val;
			unsigned char mask8;
			for (int ix = 0; ix < length; ix++) {
				val = this->uData.myParent->intUnion.at(from.getBytes()) & (1 << from.getBits());
				mask8 = 1 << to.getBits();
				((unsigned char*)&this->value)[to.getBytes()] &= (~mask8);
				if (val) {
					((unsigned char*)&this->value)[to.getBytes()] |= mask8;
				}
				from = from + inc;
				to = to + inc;
			}
		}
		else {
			for (int i = 0; i < bbSize_byte; i++) {
				*((unsigned char*)&this->value + i) =
					p->intUnion.at(i + bbOffset_byte);
			}
			signExtend(this);
		}
		if (this->typeEnum == Variable::typeEnum_t::isRef) {
			this->pointsTo = (Variable*)value;
		}
	}
	return;
}

