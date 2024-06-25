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
				haveSetId: false,
				functionArguments: opCode.functionArguments,
				returnType: opCode.returnType,
			};
		}
		case "if": {
			return getTypeOf(context, buildBlock(context, opCode.trueCodeBlock, true));
		}
		case "instance": {
			const template = build(context, opCode.template, true);
			if (!OpCode_isAtype(template)) {
				throw utilities.unreachable();
			}
			return template;
		}
		
		default: {
			throw utilities.TODO();
		}
	}
}

function getAsText(opCode: OpCode): string {
	if (OpCode_isAtype(opCode)) {
		return opCode.id;
	}
	
	switch (opCode.kind) {
		case "bool": {
			if (opCode.value) {
				return "true";
			} else {
				return "false";
			}
		}
		case "number": {
			return `${opCode.value}`;
		}
		case "string": {
			return opCode.value;
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
	
	if (expectedType.kind == "enum" && actualType.kind == "struct") {
		for (let i = 0; i < expectedType.codeBlock.length; i++) {
			const alias = expectedType.codeBlock[i];
			if (alias.kind != "alias" || alias.value.kind != "struct") {
				throw utilities.unreachable();
			}
			
			if (alias.value.id == actualType.id) {
				return;
			}
		}
	}
	
	if (expectedType.id != actualType.id) {
		let error = new CompileError(`expected type ${OpCodeType_getName(expectedType)}, but got type ${OpCodeType_getName(actualType)}`);
		callBack(error);
		throw error;
	}
}

export function buildBlock(context: BuilderContext, opCodes: OpCode[], resolve: boolean): OpCode {
	context.file.scope.levels.push(opCodes);
	
	const oldAllowTransformations = context.doTransformations;
	context.doTransformations = true;
	function end() {
		context.file.scope.levels.pop();
		context.doTransformations = oldAllowTransformations;
	}
	
	let lastOp: OpCode | null = null;
	for (let index = 0; index < opCodes.length;) {
		const opCode = opCodes[index];
		index++;
		
		lastOp = build(context, opCode, resolve);
		if (context.returnEarly) {
			context.returnEarly = false;
			end();
			return lastOp;
		}
	}
	
	for (let index = 0; index < opCodes.length; index++) {
		const opCode = opCodes[index];
		
		if (opCode.kind == "alias") {
			const value = build(context, opCode.value, true);
			if (OpCode_isAtype(value)) {
				opCode.disableGeneration = true;
			}
			if (value.kind == "function") {
				const returnType = build(context, value.returnType, true);
				if (returnType.kind == "struct" && returnType.id == "builtin:Type") {
					opCode.disableGeneration = true;
				}
			}
		}
	}
	
	if (!lastOp) {
		throw utilities.TODO();
	}
	
	end();
	return lastOp;
}

export function build(context: BuilderContext, opCode: OpCode, resolve: boolean): OpCode {
	switch (opCode.kind) {
		case "identifier": {
			const alias = getAlias(context, opCode.name);
			if (!alias) {
				throw new CompileError(`alias '${opCode.name}' does not exist`)
					.indicator(opCode.location, "here");
			}
			
			const value = build(context, alias.value, resolve);
			if (resolve) {
				return value;
			} else {
				break;
			}
		}
		case "call": {
			const functionToCall = build(context, opCode.left, true);
			
			if (functionToCall.kind != "function") {
				throw new CompileError(`call expression requires function`)
					.indicator(opCode.location, "here");
			}
			
			if (opCode.callArguments.length > functionToCall.functionArguments.length) {
				throw new CompileError(`too many arguments passed to function`)
					.indicator(opCode.location, "function call here");
			}
			
			if (opCode.callArguments.length < functionToCall.functionArguments.length) {
				throw new CompileError(`not enough arguments passed to function`)
					.indicator(opCode.location, "function call here");
			}
			
			if (resolve) {
				let argumentText = [];
				
				let newLevel: OpCode[] = [];
				for (let i = 0; i < functionToCall.functionArguments.length; i++) {
					const functionArgument = functionToCall.functionArguments[i];
					
					const value = build(context, opCode.callArguments[i], resolve);
					argumentText.push(getAsText(value));
					
					newLevel.push({
						kind: "alias",
						location: functionArgument.location,
						disableGeneration: false,
						name: functionArgument.name,
						value: opCode.callArguments[i],
					})
				}
				
				context.file.scope.levels.push(newLevel);
				const result = buildBlock(context, functionToCall.codeBlock, resolve);
				context.file.scope.levels.pop();
				
				if (OpCode_isAtype(result) && !result.haveSetId) {
					result.id = `${functionToCall.id}(${argumentText.join(", ")})`;
					result.haveSetId = true;
				}
				
				return result;
			}
			
			break;
		}
		case "builtinCall": {
			builtinCall(context, opCode);
			
			break;
		}
		case "operator": {
			if (opCode.operatorText == ".") {
				const left = build(context, opCode.left, true);
				
				if (left.kind != "struct" && left.kind != "enum" && left.kind != "instance") {
					throw utilities.TODO();
				}
				if (opCode.right.kind != "identifier") {
					throw utilities.TODO();
				}
				const name = opCode.right.name
				const alias = getAliasFromList(left.codeBlock, name);
				if (!alias) {
					throw new CompileError(`alias '${name}' does not exist on struct`)
						.indicator(opCode.right.location, "here");
				}
				return alias.value;
			} else {
				const left = build(context, opCode.left, resolve);
				const right = build(context, opCode.right, resolve);
				if (opCode.operatorText == "==") {
					if (left.kind != "complexValue" && right.kind != "complexValue") {
						if (left.kind == "number" && right.kind == "number") {
							return {
								kind: "bool",
								location: opCode.location,
								value: left.value == right.value,
							};
						} else {
							throw utilities.TODO();
						}
					}
				}
			}
			break;
		}
		case "struct": {
			if (!opCode.doCheck) {
				return opCode;
			}
			
			const outOpCode: OpCode = {
				kind: "struct",
				location: opCode.location,
				id: opCode.id,
				haveSetId: false,
				caseTag: opCode.caseTag,
				doCheck: false,
				fields: [],
				codeBlock: [],
			};
			
			for (let i = 0; i < opCode.fields.length; i++) {
				const field = opCode.fields[i];
				
				outOpCode.fields.push({
					kind: "argument",
					location: field.location,
					name: field.name,
					type: field.type,
				});
			}
			
			for (let i = 0; i < opCode.codeBlock.length; i++) {
				const alias = opCode.codeBlock[i];
				if (alias.kind != "alias") {
					throw utilities.unreachable();
				}
				outOpCode.codeBlock.push({
					kind: "alias",
					location: alias.location,
					disableGeneration: false,
					name: alias.name,
					value: build(context, alias.value, resolve),
				});
			}
			
			return outOpCode;
		}
		case "functionType": {
			const returnType = build(context, opCode.returnType, resolve);
			if (!OpCode_isAtype(returnType)) {
				throw utilities.TODO();
			}
			
			opCode.returnType = returnType;
			break;
		}
		case "function": {
			const returnType = build(context, opCode.returnType, true);
			if (!OpCode_isAtype(returnType)) {
				throw utilities.TODO();
			}
			
			let newLevel: OpCode[] = [];
			for (let i = 0; i < opCode.functionArguments.length; i++) {
				const functionArgument = opCode.functionArguments[i];
				const type = build(context, functionArgument.type, true);
				
				if (!OpCode_isAtype(type)) {
					throw new CompileError("function argument is not a type").indicator(functionArgument.location, "here");
				}
				
				newLevel.push({
					kind: "alias",
					location: functionArgument.location,
					disableGeneration: false,
					name: functionArgument.name,
					value: {
						kind: "complexValue",
						location: functionArgument.location,
						type: type,
					},
				});
			}
			context.file.scope.levels.push(newLevel);
			const result = buildBlock(context, opCode.codeBlock, true);
			context.file.scope.levels.pop();
			
			const resultType = getTypeOf(context, result);
			
			expectType(context, returnType, resultType, (error) => {
				error.indicator(opCode.returnType.location, "expected type defined here");
				error.indicator(result.location, "actual type from here");
			});
			
			opCode.returnType = returnType;
			if (opCode.doTransformations && true) {
				opCode.doTransformations = false;
				const last = opCode.codeBlock.pop();
				if (!last) {
					throw utilities.TODO();
				}
				opCode.codeBlock.push({
					kind: "return",
					location: last.location,
					expression: last,
				});
			}
			break;
		}
		case "return": {
			const value = build(context, opCode.expression, resolve);
			context.returnEarly = true;
			return value;
		}
		case "if": {
			const condition = buildBlock(context, [opCode.condition], resolve);
			
			const trueResult = buildBlock(context, opCode.trueCodeBlock, resolve);
			const trueType = getTypeOf(context, trueResult);
			const falseResult = buildBlock(context, opCode.falseCodeBlock, resolve);
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
			} else {
				if (resolve) {
					return {
						kind: "complexValue",
						location: opCode.location,
						type: trueType,
					}
				}
			}
			
			break;
		}
		case "instance": {
			const template = build(context, opCode.template, true);
			if (template.kind != "struct") {
				throw utilities.TODO();
			}
			opCode.caseTag = template.caseTag;
			
			const outOpCode: OpCode = {
				kind: "instance",
				location: opCode.location,
				caseTag: template.caseTag,
				template: template,
				codeBlock: [],
			};
			
			for (let a = 0; a < opCode.codeBlock.length; a++) {
				const alias = opCode.codeBlock[a];
				if (alias.kind != "alias") {
					throw utilities.TODO();
				}
				
				let templateField = null;
				for (let f = 0; f < template.fields.length; f++) {
					const field = template.fields[f];
					if (field.kind != "argument") {
						throw utilities.unreachable();
					}
					if (field.name == alias.name) {
						templateField = field;
						break;
					}
				}
				if (!templateField) {
					throw new CompileError(`field '${alias.name}' should not exist`)
						.indicator(alias.location, "field here")
						.indicator(template.location, `the template type does not contain a field named '${alias.name}'`);
				}
				
				const value = build(context, alias.value, resolve);
				// TODO: expectType
				outOpCode.codeBlock.push({
					kind: "alias",
					location: alias.location,
					disableGeneration: false,
					name: alias.name,
					value: value,
				});
			}
			
			for (let f = 0; f < template.fields.length; f++) {
				const field = template.fields[f];
				if (field.kind != "argument") {
					throw utilities.unreachable();
				}
				
				let foundField = false;
				for (let a = 0; a < opCode.codeBlock.length; a++) {
					const alias = opCode.codeBlock[a];
					if (alias.kind != "alias") {
						throw utilities.unreachable();
					}
					
					if (field.name == alias.name) {
						foundField = true;
						break;
					}
				}
				
				if (!foundField) {
					throw new CompileError(`struct instance is missing field '${field.name}'`)
						.indicator(opCode.location, "struct instance here")
						.indicator(field.location, "missing field defined here");
				}
			}
			
			return outOpCode;
		}
		case "alias": {
			const alias = getAlias(context, opCode.name);
			if (alias && alias != opCode) {
				throw new CompileError(`alias '${opCode.name}' already exists`)
					.indicator(opCode.location, "alias redefined here")
					.indicator(alias.location, "alias originally defined here");
			}
			
			opCode.value = build(context, opCode.value, false);
			if (opCode.value.kind == "function" && !opCode.value.haveSetId) {
				opCode.value.id = `${opCode.name}`;
				opCode.value.haveSetId = true;
			}
			
			break;
		}
	}
	
	return opCode;
}