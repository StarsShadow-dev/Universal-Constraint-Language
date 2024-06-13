import utilities from "./utilities";
import { CompileError } from "./report";
import { BuilderContext } from "./compiler";
import { OpCode, OpCodeType, OpCodeType_getName, OpCode_alias, OpCode_isAtype } from "./types";
import { builtinCall, builtinTypes } from "./builtin";

export function getAlias(context: BuilderContext, name: string): OpCode_alias | null {
	{
		const alias = getAliasFromList(builtinTypes, name);
		if (alias) return alias;
	}
	
	for (let i = context.file.scope.levels.length - 1; i >= 0; i--) {
		const alias = getAliasFromList(context.file.scope.levels[i], name);
		if (alias) return alias;
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
	
	switch (opCode.kind) {
		case "bool": {
			return castToValue(getAliasFromList(builtinTypes, "Bool"));
		}
		
		case "number": {
			return castToValue(getAliasFromList(builtinTypes, "Number"));
		}
		
		case "string": {
			return castToValue(getAliasFromList(builtinTypes, "String"));
		}
		
		default: {
			throw utilities.TODO();
		}
	}
}

/**
 * Takes an expectedType and an actualType, if the types are different then it throws an error.
 * 
 * @param callBack Used to modify the error message to add more context.
 */
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

export function buildBlock(context: BuilderContext, opCodes: OpCode[]): OpCode {
	context.file.scope.levels.push(opCodes);
	context.file.scope.currentLevel++;
	
	let lastOp: OpCode | null = null;
	
	for (let index = 0; index < opCodes.length;) {
		const opCode = opCodes[index];
		index++;
		
		lastOp = build(context, opCode);
	}
	
	if (context.compilerOptions.builderTransforms.removeTypes) {
		for (let index = 0; index < opCodes.length; index++) {
			const opCode = opCodes[index];
			
			if (opCode.kind == "alias") {
				const value = build(context, opCode.value);
				if (OpCode_isAtype(value)) {
					opCodes.splice(index, 1);
				}
			}
		}
	}
	
	if (!lastOp) {
		throw utilities.TODO();
	}
	
	context.file.scope.levels.pop();
	context.file.scope.currentLevel--;
	return lastOp;
}

export function build(context: BuilderContext, opCode: OpCode): OpCode {
	switch (opCode.kind) {
		case "identifier": {
			const alias = getAlias(context, opCode.name);
			if (!alias) {
				throw utilities.TODO();
			}
			
			const value = build(context, alias.value);
			return value;
		}
		case "builtinCall": {
			builtinCall(context, opCode);
			
			break;
		}
		case "alias": {
			build(context, opCode.value);
			
			break;
		}
		case "function": {
			const result = buildBlock(context, opCode.codeBlock);
			const resultType = getTypeOf(context, result);
			
			const returnType = build(context, opCode.returnType);
			if (!OpCode_isAtype(returnType)) {
				throw utilities.TODO();
			}
			
			expectType(context, returnType, resultType, (error) => {
				error.indicator(opCode.returnType.location, "expected type defined here");
				error.indicator(result.location, "actual type from here");
			});
			
			break;
		}
		case "if": {
			const condition = buildBlock(context, [opCode.condition]);
			
			const trueResult = buildBlock(context, opCode.trueCodeBlock);
			const trueType = getTypeOf(context, trueResult);
			const falseResult = buildBlock(context, opCode.falseCodeBlock);
			const falseType = getTypeOf(context, falseResult);
			
			expectType(context, trueType, falseType, (error) => {
				error.indicator(trueResult.location, `expected same type as trueCodeBlock (${OpCodeType_getName(trueType)})`);
				error.indicator(falseResult.location, `but got type ${OpCodeType_getName(falseType)}`);
			});
			
			if (condition.kind == "bool") {
				if (condition.value) {
					return trueResult;
				} else {
					return falseResult;
				}
			}
			
			break;
		}
	}
	
	return opCode;
}