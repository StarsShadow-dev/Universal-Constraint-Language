import crypto from "crypto";

import * as utilities from "./utilities.js";
import logger, { LogType } from "./logger.js";
import { CompileError, Indicator } from "./report.js";
import { builtinTypes, getBuiltinType, isBuiltinType } from "./builtin.js";
import { SourceLocation } from "./types.js";
import { ASTnode, ASTnode_alias, ASTnode_error, ASTnode_identifier, ASTnodeType, ASTnodeType_functionType, ASTnodeType_selfType, evaluateList } from "./ASTnodes.js";

export type topLevelDef = {
	// uuid: string,
	name: string,
	value: ASTnode,
};

export type DB = {
	defs: topLevelDef[],
	// changeLog
	topLevelEvaluations: Indicator[],
	errors: CompileError[],
};
export function makeDB(): DB {
	return {
		defs: [],
		topLevelEvaluations: [],
		errors: [],
	};
}

type Hash = string;
function hashString(text: string): Hash {
	return crypto.createHash("sha256").update(text).digest("hex");
}
function getUUID(): string {
	const byteSize = 10;
	const bytes = crypto.getRandomValues(new Uint8Array(byteSize));
	let string = "";
	for (let i = 0; i < bytes.length; i++) {
		const byteString = bytes[i].toString(16).toUpperCase();
		if (byteString.length == 1) {
			string += "0";
		}
		string += byteString;
	}
	return string;
}

export function getDefFromList(list: topLevelDef[], name: string): topLevelDef | null {
	for (let i = 0; i < list.length; i++) {
		const def = list[i];
		if (def.value == null) {
			utilities.unreachable();
		}
		if (def.name == name) {
			return def as topLevelDef;
		}
	}

	return null;
}

export type BuilderContext = {
	db: DB,
	aliases: ASTnode_alias[],
	setUnalias: boolean,
	
	// Resolve is for a case like this:
	// @x(Number) x + (1 + 1)
	// If the function is a evaluated at top level, I want the AST to stay the same.
	// If resolve always happened the output of that expression would be:
	// @x(Number) x + 2
	// Which is probably fine in this case, but for more complex expressions,
	// having everything be simplified can make things completely unreadable.
	resolve: boolean,
};
export function makeBuilderContext(db: DB): BuilderContext {
	return {
		db: db,
		aliases: [],
		setUnalias: false,
		resolve: true,
	};
}

export function getAlias(context: BuilderContext, name: string): ASTnode_alias | null {
	for (let i = context.aliases.length-1; i >= 0; i--) {
		const alias = context.aliases[i];
		if (alias.left instanceof ASTnode_identifier && alias.left.name == name) {
			return alias;
		}
	}
	
	for (let i = 0; i < builtinTypes.length; i++) {
		const alias = builtinTypes[i];
		if (alias.left instanceof ASTnode_identifier && alias.left.name == name) {
			return alias;
		}
	}
	
	return null;
}

export function getAliasFromList(AST: ASTnode[], name: string): ASTnode_alias | null {
	for (let i = 0; i < AST.length; i++) {
		const alias = AST[i];
		if (!(alias instanceof ASTnode_alias)) {
			utilities.unreachable();
		}
		if (alias.left instanceof ASTnode_identifier && alias.left.name == name) {
			return alias;
		}
	}

	return null;
}

// export function withAlias<T>(context: BuilderContext, newAlias: ASTnode_alias, callBack: () => T): T {
// 	context.aliases.push(newAlias);
// 	const result = callBack();
// 	context.aliases.pop();
// 	return result;
// }

export function unAlias(context: BuilderContext, name: string): ASTnode | null {
	{
		const alias = getDefFromList(context.db.defs, name);
		if (alias) {
			return alias.value;
		}
	}
	
	{
		const alias = getAlias(context, name);
		if (alias) {
			return alias.value;
		} else {
			return null;
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
	expectedType: ASTnodeType,
	actualType: ASTnodeType,
): CompileError | null {
	if (expectedType instanceof ASTnodeType_selfType) {
		return null;
	}
	
	if (isBuiltinType(expectedType, "Function") && actualType instanceof ASTnodeType_functionType) {
		return null;
	}
	
	// if (expectedType.kind == "functionType" && actualType.kind == "functionType") {
	// 	if (!OpCode_isAtype(expectedType.returnType) || !OpCode_isAtype(actualType.returnType)) {
	// 		utilities.TODO();
	// 	}
	// 	expectType(context, expectedType.returnType, actualType.returnType, (error) => {
	// 		error.msg =
	// 		`expected type \"${OpCodeType_getName(expectedType)}\", but got type \"${OpCodeType_getName(actualType)}\"\n  return type `
	// 		+ error.msg;
			
	// 		callBack(error);
	// 	});
	// 	return;
	// }
	
	// if (expectedType.kind == "enum" && actualType.kind == "struct") {
	// 	for (let i = 0; i < expectedType.codeBlock.length; i++) {
	// 		const alias = expectedType.codeBlock[i];
	// 		if (alias.kind != "alias" || alias.value.kind != "struct") {
	// 			utilities.unreachable();
	// 		}
			
	// 		if (alias.value.id == actualType.id) {
	// 			return;
	// 		}
	// 	}
	// }
	
	if (expectedType.id != actualType.id) {
		const expectedTypeText = expectedType.print();
		const actualTypeText = actualType.print();
		const error = new CompileError(`expected type ${expectedTypeText}, but got type ${actualTypeText}`);
		debugger;
		return error;
	}
	
	return null;
}

export function makeAliasWithType(location: SourceLocation, name: string, type: ASTnodeType): ASTnode_alias {
	return new ASTnode_alias(
		location,
		new ASTnode_identifier(location, name),
		new ASTnodeType_selfType(location, "TODO?", type)
	);
}

// export function getType(context: BuilderContext, node: ASTnode): ASTnodeType | ASTnode_error {
// 	switch (node.kind) {
// 		case "identifier": {
// 			const def = unAlias(context, node.name);
// 			if (!def) {
// 				return ASTnode_error_new(node.location,
// 					new CompileError(`alias '${node.name}' does not exist`)
// 						.indicator(node.location, `here`)
// 				);
// 			}
			
// 			return getType(context, def);
// 		}
		
// 		case "call": {
// 			const leftType = getType(context, node.left);
// 			if (leftType.kind == "error") return leftType;
// 			if (leftType.kind != "functionType" || !ASTnode_isAtype(leftType.returnType)) {
// 				const error = new CompileError(`can not call type ${justPrint(leftType)}`)
// 					.indicator(node.left.location, `here`);
// 				return ASTnode_error_new(node.location, error);
// 			}
			
// 			const functionToCall = evaluate(context, node.left);
// 			if (functionToCall.kind != "function") {
// 				throw utilities.TODO_addError();
// 			}
// 			const functionToCallArgType = evaluate(context, functionToCall.arg.type);
// 			if (!ASTnode_isAtype(functionToCallArgType)) {
// 				utilities.TODO_addError();
// 			}
			
// 			const actualArgType = getType(context, node.arg);
// 			if (actualArgType.kind == "error") {
// 				return actualArgType;
// 			}
			
// 			{
// 				const error = expectType(context, functionToCallArgType, actualArgType);
// 				if (error) {
// 					error.indicator(node.location, `for function call here`);
// 					error.indicator(node.arg.location, `(this argument)`);
// 					error.indicator(functionToCall.location, `function from here`);
// 					return ASTnode_error_new(node.location, error);
// 				}
// 			}
			
// 			const arg = node.arg;
			
// 			const newNode = evaluate(context, node);
// 			context.aliases.push(makeAliasWithType(arg.location, functionToCall.arg.name, functionToCallArgType));
// 			const returnType = getType(context, newNode);
// 			context.aliases.pop();
			
// 			return returnType;
// 		}
		
// 		case "builtinCall": {
// 			return getType(context, evaluateBuiltin(context, node));
// 		}
		
// 		case "operator": {
// 			const op = node.operatorText;
			
// 			if (op == "+" || op == "-") {
// 				const left = getType(context, node.left);
// 				if (left.kind == "error") {
// 					return left;
// 				}
// 				const right = getType(context, node.right);
// 				if (right.kind == "error") {
// 					return right;
// 				}
				
// 				{
// 					let error = expectType(context, getBuiltinType("Number"), left);
// 					if (error) {
// 						error.indicator(node.left.location, `on left of '${op}' operator`);
// 						return ASTnode_error_new(node.location, error);
// 					}
// 				}
// 				{
// 					let error = expectType(context, getBuiltinType("Number"), right);
// 					if (error) {
// 						error.indicator(node.right.location, `on right of '${op}' operator`);
// 						return ASTnode_error_new(node.location, error);
// 					}
// 				}
				
// 				return getBuiltinType("Number");
// 			} else if (op == ".") {
// 				const left = evaluate(context, node.left);
// 				if (left.kind != "instance") {
// 					utilities.TODO_addError();
// 				}
				
// 				if (node.right.kind != "identifier") {
// 					utilities.TODO_addError();
// 				}
// 				const name = node.right.name;
				
// 				const alias = getAliasFromList(left.codeBlock, name);
// 				if (!alias) {
// 					utilities.TODO_addError();
// 				}
				
// 				return getType(context, alias.value);
// 			} else {
// 				utilities.TODO();
// 			}
// 		}
		
// 		case "struct": {
// 			return getBuiltinType("Type");
// 		}
		
// 		case "functionType": {
// 			return node;
// 		}
		
// 		case "function": {
// 			const arg = node.arg;
// 			let argumentType = evaluate(context, arg.type);
// 			if (!ASTnode_isAtype(argumentType)) {
// 				// TODO: errors?
// 				argumentType = getBuiltinType("Any");
// 			}
// 			context.aliases.push(makeAliasWithType(arg.location, arg.name, argumentType));
// 			const actualResultType = getTypeFromList(context, node.body);
// 			context.aliases.pop();
// 			if (actualResultType.kind == "error") {
// 				return actualResultType;
// 			}
			
// 			return {
// 				kind: "functionType",
// 				location: node.location,
// 				id: `${JSON.stringify(node.location)}`,
// 				arg: argumentType,
// 				returnType: actualResultType,
// 			};
// 		}
		
// 		case "if": {
// 			const condition = getType(context, node.condition);
// 			if (condition.kind == "error") {
// 				return condition;
// 			}
			
// 			const trueType = getType(context, node.trueBody);
// 			if (trueType.kind == "error") {
// 				return trueType;
// 			}
// 			const falseType = getType(context, node.falseBody);
// 			if (falseType.kind == "error") {
// 				return falseType;
// 			}
			
// 			{
// 				const error = expectType(context, trueType, falseType);
// 				if (error) {
// 					const trueLocation = node.trueBody.location;
// 					const falseLocation = node.falseBody.location;
// 					error.indicator(trueLocation, `expected same type as trueBody (${printASTnode({level:0}, trueType)})`);
// 					error.indicator(falseLocation, `but got type ${printASTnode({level:0}, falseType)}`);
// 					return ASTnode_error_new(node.location, error);
// 				}
// 			}
			
// 			return trueType;
// 		}
		
// 		case "instance": {
// 			const template = evaluate(context, node.template);
// 			if (!ASTnode_isAtype(template)) {
// 				utilities.TODO_addError();
// 			}
// 			return template;
// 		}
		
// 		case "_selfType": {
// 			return node.type;
// 		}
// 	}
	
// 	utilities.unreachable();
// }

export function getTypeFromList(context: BuilderContext, AST: ASTnode[]): ASTnodeType | ASTnode_error {
	let outNode = null;
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		if (ASTnode instanceof ASTnode_alias) {
			const valueType = ASTnode.value.getType(context);
			if (valueType instanceof ASTnode_error) {
				return valueType;
			}
			const name = ASTnode.left.print();
			context.aliases.push(makeAliasWithType(ASTnode.location, name, valueType));
		} else {
			outNode = ASTnode.getType(context);
		}
		// if (ASTnode.kind == "identifier") {
		// 	const alias = getAlias(context, ASTnode.name);
		// 	if (alias) {
		// 		if (alias.unalias) {
		// 			logger.log(LogType.build, `unalias ${ASTnode.name}`);
		// 			AST[i] = alias.value[0];
		// 		} else {
		// 			logger.log(LogType.build, `no unalias ${ASTnode.name}`);
		// 		}
		// 	}
		// }
	}
	
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		if (ASTnode instanceof ASTnode_alias) {
			context.aliases.pop();
		}
	}
	
	if (!outNode) utilities.unreachable();
	return outNode;
}

export function addToDB(db: DB, AST: ASTnode[]) {
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		
		if (ASTnode instanceof ASTnode_alias) {
			// const hash = hashString(JSON.stringify(ASTnode.value)); // TODO: locations...
			// const uuid = getUUID();
			const name = ASTnode.left.print();
			db.defs.push({
				// uuid: uuid,
				name: name,
				value: ASTnode.value,
			});
			logger.log(LogType.addToDB, `added ${ASTnode.left}`);
		}
	}
	
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		
		if (ASTnode instanceof ASTnode_alias) {
			const name = ASTnode.left.print();
			const def = getDefFromList(db.defs, name);
			if (!def) utilities.unreachable();
			
			const error = getTypeFromList(makeBuilderContext(db), [ASTnode.value]);
			if (error instanceof ASTnode_error && error.compileError) {
				db.errors.push(error.compileError);
				break;
			}
			const value = ASTnode.value.evaluate(makeBuilderContext(db));
			
			if (value instanceof ASTnodeType) {
				value.id = def.name;
			}
			def.value = value;
		} else {
			logger.log(LogType.addToDB, `found top level evaluation`);
			
			const location = ASTnode.location;
			{
				const error = getTypeFromList(makeBuilderContext(db), [ASTnode]);
				if (error instanceof ASTnode_error) {
					if (!error.compileError) {
						logger.log(LogType.addToDB, `!error.compileError ???`);
						break;
					}
					db.errors.push(error.compileError);
					continue;
				}
			}
			
			const result = evaluateList(makeBuilderContext(db), [ASTnode])[0];
			const resultText = result.print();
			
			db.topLevelEvaluations.push({ location: location, msg: `${resultText}` });
		}
	}
}