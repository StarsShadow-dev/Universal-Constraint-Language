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
		
		case "complexValue": {
			return opCode.type;
		}
		
		case "struct": {
			return castToValue(getAliasFromList(builtinTypes, "Type"));
		}
		
		case "function": {
			return {
				kind: "functionType",
				location: opCode.location,
				id: `${JSON.stringify(opCode.location)}`,
				functionArguments: opCode.functionArguments,
				returnType: opCode.returnType,
			};
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
	if (expectedType.kind == "functionType" && actualType.kind == "functionType") {
		if (!OpCode_isAtype(expectedType.returnType) || !OpCode_isAtype(actualType.returnType)) {
			throw utilities.TODO();
		}
		expectType(context, expectedType.returnType, actualType.returnType, (error) => {
			error.msg =
			`expected type \"${OpCodeType_getName(expectedType)}\", but got type \"${OpCodeType_getName(actualType)}\"\n  return type `
			+ error.msg;
			
			callBack(error);
		});
		return;
	}
	
	if (expectedType.id != actualType.id) {
		let error = new CompileError(`expected type ${OpCodeType_getName(expectedType)}, but got type ${OpCodeType_getName(actualType)}`);
		callBack(error);
		throw error;
	}
}

export function buildBlock(context: BuilderContext, opCodes: OpCode[]): OpCode {
	context.file.scope.levels.push(opCodes);
	
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
	return lastOp;
}

// export function buildNewLevel(context: BuilderContext, opCode: OpCode, levels: OpCode[][]): OpCode {
// 	const oldScope = context.file.scope;
// 	context.file.scope = {
// 		levels: levels,
// 		function: null,
// 		functionArgumentNameText: "",
// 	};
// 	const result = build(context, opCode);
// 	context.file.scope = oldScope;
	
// 	return result;
// }

export function build(context: BuilderContext, opCode: OpCode): OpCode {
	switch (opCode.kind) {
		case "identifier": {
			const alias = getAlias(context, opCode.name);
			if (!alias) {
				throw new CompileError(`alias '${opCode.name}' does not exist`).indicator(opCode.location, "here");
			}
			
			const value = build(context, alias.value);
			return value;
		}
		case "builtinCall": {
			builtinCall(context, opCode);
			
			break;
		}
		case "struct": {
			if (opCode.codeBlock.length > 0) {
				buildBlock(context, opCode.codeBlock);
			}
			break;
		}
		case "functionType": {
			const returnType = build(context, opCode.returnType);
			if (!OpCode_isAtype(returnType)) {
				throw utilities.TODO();
			}
			
			opCode.returnType = returnType;
			break;
		}
		case "operator": {
			if (opCode.operatorText == ".") {
				if (opCode.right.kind != "identifier") {
					throw utilities.TODO();
				}
				
				const left = build(context, opCode.left);
				if (left.kind != "struct") {
					throw utilities.TODO();
				}
				const name = opCode.right.name;
				
				const alias = getAliasFromList(left.codeBlock, name);
				if (!alias) {
					throw utilities.TODO();
				}
				
				const value = build(context, alias.value);
				return value;
			}
			break;
		}
		case "alias": {
			opCode.value = build(context, opCode.value);
			
			break;
		}
		case "function": {
			const returnType = build(context, opCode.returnType);
			if (!OpCode_isAtype(returnType)) {
				throw utilities.TODO();
			}
			
			let newLevel: OpCode[] = [];
			for (let i = 0; i < opCode.functionArguments.length; i++) {
				const functionArgument = opCode.functionArguments[i];
				const type = build(context, functionArgument.type);
				
				if (!OpCode_isAtype(type)) {
					throw new CompileError("function argument is not a type").indicator(functionArgument.location, "here");
				}
				
				newLevel.push({
					kind: "alias",
					location: functionArgument.location,
					name: functionArgument.name,
					value: {
						kind: "complexValue",
						location: functionArgument.location,
						type: type,
					},
				})
			}
			context.file.scope.levels.push(newLevel);
			const result = buildBlock(context, opCode.codeBlock);
			context.file.scope.levels.pop();
			
			const resultType = getTypeOf(context, result);
			
			expectType(context, returnType, resultType, (error) => {
				error.indicator(opCode.returnType.location, "expected type defined here");
				error.indicator(result.location, "actual type from here");
			});
			
			opCode.returnType = returnType;
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