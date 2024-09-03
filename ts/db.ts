import crypto from "crypto";

import * as utilities from "./utilities.js";
import logger, { LogType } from "./logger.js";
import { CompileError, Indicator } from "./report.js";
import { builtinTypes, isBuiltinType } from "./builtin.js";
import { SourceLocation } from "./types.js";
import { ASTnode, ASTnode_alias, ASTnode_error, ASTnode_function, ASTnode_identifier, ASTnodeType, ASTnodeType_functionType, ASTnodeType_selfType, evaluateList, getTypeFromList } from "./ASTnodes.js";

export type topLevelDef = {
	// uuid: string,
	name: string,
	value: ASTnode,
	hasError: boolean,
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

// export function getType(context: BuilderContext, node: ASTnode): ASTnodeType | ASTnode_error {
// 	switch (node.kind) {
// 		case "struct": {
// 			return getBuiltinType("Type");
// 		}
		
// 		case "functionType": {
// 			return node;
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

export function addToDB(db: DB, AST: ASTnode[]) {
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		
		if (ASTnode instanceof ASTnode_alias) {
			// const hash = hashString(JSON.stringify(ASTnode.value));
			// const uuid = getUUID();
			let hasError = false;
			const name = ASTnode.left.print();
			{
				const error = ASTnode.getType(makeBuilderContext(db));
				if (error instanceof ASTnode_error && error.compileError) {
					db.errors.push(error.compileError);
					hasError = true;
				}
			}
			db.defs.push({
				// uuid: uuid,
				name: name,
				value: ASTnode.value,
				hasError: hasError,
			});
			logger.log(LogType.addToDB, `added ${name}`);
		}
	}
	
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		
		if (ASTnode instanceof ASTnode_alias) {
			const name = ASTnode.left.print();
			const def = getDefFromList(db.defs, name);
			if (!def) utilities.unreachable();
			if (def.hasError) continue;
			
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
				const error = ASTnode.getType(makeBuilderContext(db));
				if (error instanceof ASTnode_error) {
					if (!error.compileError) {
						logger.log(LogType.addToDB, `!error.compileError ???`);
						break;
					}
					db.errors.push(error.compileError);
					continue;
				}
			}
			
			const result = ASTnode.evaluate(makeBuilderContext(db));
			const resultText = result.print();
			
			db.topLevelEvaluations.push({ location: location, msg: `${resultText}` });
		}
	}
}