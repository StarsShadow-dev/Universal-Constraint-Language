import utilities from "./utilities";
import { CompileError } from "./report";
import { BuilderContext } from "./compiler";
import { OpCode, OpCodeType, OpCodeType_getName, OpCode_alias, OpCode_isAtype } from "./types";
import { builtinTypes } from "./builtin";

export function getAlias(context: BuilderContext, name: string): OpCode_alias | null {
	{
		const alias = getAliasFromList(builtinTypes, name);
		if (alias) return alias;
	}
	
	for (let i = 0; i < context.file.scope.levels.length; i++) {
			
	}
	
	return null;
}

export function getAliasFromList(opCodes: OpCode[], name: string): OpCode_alias | null {
	for (let i = 0; i < opCodes.length; i++) {
		const opCode = opCodes[i];
		if (opCode.kind == "alias") {
			if (opCode.name == name) {
				return opCode;
			}
		}
	}
	
	return null;
}

function getTypeOf(context: BuilderContext, opCode: OpCode): OpCodeType {
	function castToValue(alias: OpCode_alias | null): OpCodeType {
		if (!alias || !OpCode_isAtype(alias.value)) {
			throw utilities.unreachable();
		}
		
		return alias.value;
	}
	
	if (opCode.kind == "bool") {
		return castToValue(getAliasFromList(builtinTypes, "Bool"));
	} else if (opCode.kind == "number") {
		return castToValue(getAliasFromList(builtinTypes, "Number"));
	} else if (opCode.kind == "string") {
		return castToValue(getAliasFromList(builtinTypes, "String"));
	} else {
		throw utilities.TODO();
	}
}

// Takes an expectedType and an actualType, if the types are different then throws an error.
// 'callBack' is used to modify the error message to add more context.
function expectType(
	context: BuilderContext,
	expectedType: OpCodeType,
	actualType: OpCodeType,
	callBack: (error: CompileError) => void,
) {
	if (expectedType.id != actualType.id) {
		let error = new CompileError(`expected type ${OpCodeType_getName(expectedType)}, but got type ${OpCodeType_getName(actualType)}`);
		callBack(error);
		throw error;
	}
}

// To make an OpCode no longer context dependent.
function resolveOp(context: BuilderContext, opCode: OpCode): OpCode {
	if (opCode.kind == "identifier") {
		const alias = getAlias(context, opCode.name);
		if (!alias) {
			throw utilities.TODO();
		}
		
		return alias.value;
	}
	
	return opCode;
}

export function build(context: BuilderContext, opCodes: OpCode[]): OpCode {
	context.file.scope.levels.push(opCodes);
	context.file.scope.currentLevel++;
	
	let index = 0
	while (index < opCodes.length) {
		const opCode = opCodes[index];
		index++;
		
		typeCheck(context, opCode);
	}
	
	const lastOp = opCodes[index-1];
	if (!lastOp) {
		utilities.unreachable();
	}
	
	context.file.scope.levels.pop();
	context.file.scope.currentLevel--;
	
	return resolveOp(context, lastOp);
}

export function typeCheck(context: BuilderContext, opCode: OpCode) {
	switch (opCode.kind) {
		case "alias": {
			build(context, [opCode.value]);
			break;
		}
		case "function": {
			const result = build(context, opCode.codeBlock);
			const resultType = getTypeOf(context, result);
			
			const returnType = resolveOp(context, opCode.returnType);
			if (!OpCode_isAtype(returnType)) {
				throw utilities.unreachable();
			}
			
			debugger;
			
			expectType(context, returnType, resultType, (error) => {
				error.indicator(opCode.returnType.location, "expected type defined here");
				error.indicator(result.location, "actual type from here");
			});
			
			break;
		}
	}
}