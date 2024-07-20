import crypto from "crypto";

import utilities from "./utilities";
import logger, { LogType } from "./logger";
import { evaluate } from "./evaluate";
import { printASTnode } from "./printAST";
import { CompileError, Indicator } from "./report";
import { evaluateBuiltin, builtinTypes, getBuiltinType, isBuiltinType } from "./builtin";
import {
	ASTnode,
	ASTnodeType,
	ASTnode_alias,
	ASTnode_error,
	ASTnode_error_new,
	ASTnode_isAtype,
} from "./parser";

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
			throw utilities.unreachable();
		}
		if (def.name == name) {
			return def as topLevelDef;
		}
	}

	return null;
}

export type BuilderContext = {
	db: DB,
	levels: ASTnode_alias[][],
};

export function unAlias(context: BuilderContext, name: string): ASTnode | null {
	{
		const alias = getDefFromList(context.db.defs, name);
		if (alias) {
			return alias.value;
		}
	}
	
	for (let i = 0; i < context.levels.length; i++) {
		for (let j = 0; j < context.levels[i].length; j++) {
			const alias = context.levels[i][j];
			if (alias.name == name) {
				return alias.value;
			}
		}
	}
	
	for (let i = 0; i < builtinTypes.length; i++) {
		const alias = builtinTypes[i];
		if (alias.name == name) {
			return alias.value;
		}
	}
	
	return null;
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
	if (isBuiltinType(expectedType, "Function") && actualType.kind == "functionType") {
		return null;
	}
	
	// if (expectedType.kind == "functionType" && actualType.kind == "functionType") {
	// 	if (!OpCode_isAtype(expectedType.returnType) || !OpCode_isAtype(actualType.returnType)) {
	// 		throw utilities.TODO();
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
	// 			throw utilities.unreachable();
	// 		}
			
	// 		if (alias.value.id == actualType.id) {
	// 			return;
	// 		}
	// 	}
	// }
	
	if (expectedType.id != actualType.id) {
		const expectedTypeText = printASTnode({level: 0}, expectedType);
		const actualTypeText = printASTnode({level: 0}, actualType);
		return new CompileError(`expected type ${expectedTypeText}, but got type ${actualTypeText}`);
	}
	
	return null;
}

export function build(context: BuilderContext, node: ASTnode): ASTnodeType | ASTnode_error {
	switch (node.kind) {
		case "bool": {
			return getBuiltinType("Bool");
		}
		case "number": {
			return getBuiltinType("Number");
		}
		case "string": {
			return getBuiltinType("String");
		}
		
		case "identifier": {
			const def = unAlias(context, node.name);
			if (!def) {
				throw utilities.TODO();
			}
			
			return build(context, def);
		}
		
		case "call": {
			const left = evaluate(context, node.left);
			if (left.kind != "function") {
				throw utilities.TODO();
			}
			const leftType = build(context, left);
			if (leftType.kind == "error") return leftType;
			if (leftType.kind != "functionType" || !ASTnode_isAtype(leftType.returnType)) {
				throw utilities.unreachable();
			}
			
			return leftType.returnType;
		}
		
		case "builtinCall": {
			return build(context, evaluateBuiltin(context, node));
		}
		
		case "operator": {
			const op = node.operatorText;
			
			if (op == "+" || op == "-") {
				const left = build(context, node.left);
				if (left.kind == "error") {
					return left;
				}
				const right = build(context, node.right);
				if (right.kind == "error") {
					return right;
				}
				
				{
					let error = expectType(context, getBuiltinType("Number"), left);
					if (error) {
						error.indicator(node.left.location, `on left of '${op}' operator`);
						return ASTnode_error_new(node.location, error);
					}
				}
				{
					let error = expectType(context, getBuiltinType("Number"), right);
					if (error) {
						error.indicator(node.right.location, `on right of '${op}' operator`);
						return ASTnode_error_new(node.location, error);
					}
				}
				
				return getBuiltinType("Number");
			} else {
				throw utilities.TODO();
			}
		}
		
		case "struct": {
			return getBuiltinType("Type");
		}
		
		case "functionType": {
			return node;
		}
		
		case "function": {
			const expectedReturnType = evaluate(context, node.returnType);
			if (!ASTnode_isAtype(expectedReturnType)) {
				throw utilities.TODO();
			}
			
			const actualResultType = buildList(context, node.codeBlock);
			if (actualResultType.kind == "error") {
				return actualResultType;
			}
			
			{
				let error = expectType(context, expectedReturnType, actualResultType);
				if (error) {
					error.indicator(node.returnType.location, "expected type from here");
					return ASTnode_error_new(node.location, error);
				}
			}
			
			let functionArguments = [];
			for (let i = 0; i < node.functionArguments.length; i++) {
				const value = build(context, node.functionArguments[i].type)
				if (value.kind == "error") {
					return value;
				}
				if (!ASTnode_isAtype(value)) {
					throw utilities.TODO();
				}
				functionArguments.push(value);
			}
			
			return {
				kind: "functionType",
				location: node.location,
				id: `${JSON.stringify(node.location)}`,
				functionArguments: functionArguments,
				returnType: expectedReturnType,
			};
		}
		
		case "instance": {
			const template = build(context, node.template);
			return template;
		}
	}
	
	throw utilities.unreachable();
}

export function buildList(context: BuilderContext, AST: ASTnode[]): ASTnodeType | ASTnode_error {
	let node = null;
	for (let i = 0; i < AST.length; i++) {
		node = build(context, AST[i]);
	}
	
	if (!node) throw utilities.unreachable();
	return node;
}

export function addToDB(db: DB, AST: ASTnode[]) {
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		
		if (ASTnode.kind == "alias") {
			// const hash = hashString(JSON.stringify(ASTnode.value)); // TODO: locations...
			// const uuid = getUUID();
			db.defs.push({
				// uuid: uuid,
				name: ASTnode.name,
				value: ASTnode.value,
			});
			logger.log(LogType.addToDB, `added ${ASTnode.name}`);
		}
	}
	
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		
		if (ASTnode.kind == "alias") {
			const def = getDefFromList(db.defs, ASTnode.name);
			if (!def) throw utilities.unreachable();
			
			const value = ASTnode.value;
			const error = buildList({ db: db, levels: [] }, [value]);
			if (error.kind == "error" && error.compileError) {
				db.errors.push(error.compileError);
				break; // ?
			}
			
			if (ASTnode_isAtype(value)) {
				value.id = def.name;
			}
			def.value = value;
		} else {
			logger.log(LogType.addToDB, `found top level evaluation`);
			
			const location = ASTnode.location;
			const error = buildList({ db: db, levels: [] }, [ASTnode]);
			if (error.kind == "error") {
				if (!error.compileError) break;
				db.errors.push(error.compileError);
				continue;
			}
			const result = evaluate({
				db: db,
				levels: [],
			}, ASTnode);
			const resultText = printASTnode({level: 0}, result);
			
			db.topLevelEvaluations.push({ location: location, msg: `${resultText}` });
		}
	}
}