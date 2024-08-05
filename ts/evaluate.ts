import utilities from "./utilities";
import { BuilderContext, getAlias, unAlias } from "./db";
import { ASTnode, ASTnode_argument, ASTnode_function, ASTnode_isAtype } from "./parser";
import { evaluateBuiltin, getBuiltinType } from "./builtin";
import logger, { LogType } from "./logger";
import { printASTnode } from "./printAST";

export function evaluate(context: BuilderContext, node: ASTnode): ASTnode {
	switch (node.kind) {
		case "identifier": {
			const value = unAlias(context, node.name);
			if (!value) throw utilities.unreachable();
			
			if (context.resolve) {
				return evaluate(context, value);
			} else {
				const alias = getAlias(context, node.name);
				if (alias && alias.unalias) {
					return evaluate(context, value);
				} else {
					return node;
				}
			}
		}
		
		case "call": {
			const oldResolve = context.resolve;
			context.resolve = true;
			const functionToCall = evaluateList(context, [node.left])[0];
			context.resolve = oldResolve;
			if (functionToCall.kind != "function") {
				throw utilities.TODO();
			}
			
			const oldSetUnalias = context.setUnalias;
			context.levels.push([]);
			context.setUnalias = true;
			const arg = functionToCall.arg;
			const argValue = evaluate(context, node.arg);
			context.levels[context.levels.length-1].push({
				kind: "alias",
				location: arg.location,
				unalias: true,
				name: arg.name,
				value: argValue,
			});
			const result = evaluate(context, functionToCall.body);
			context.levels.pop();
			context.setUnalias = oldSetUnalias;
			
			return result;
		}
		
		case "builtinCall": {
			return evaluateBuiltin(context, node);
		}
		
		case "operator": {
			const op = node.operatorText;
			if (op == "+" || op == "-") {
				const left = evaluate(context, node.left);
				const right = evaluate(context, node.right);
				if (left.kind != "number" || right.kind != "number") {
					return node;
					// return {
					// 	kind: "_selfType",
					// 	location: node.location,
					// 	id: "TODO?",
					// 	type: getBuiltinType("Number"),
					// };
				}
				const left_v = left.value;
				const right_v = right.value;
				
				let result: number;
				if (op == "+") {
					result = left_v + right_v;
				} else if (op == "-") {
					result = left_v - right_v;
				} else {
					throw utilities.unreachable();
				}
				
				return {
					kind: "number",
					location: node.location,
					value: result,
				};
			} else {
				throw utilities.TODO();
			}
		}
		
		case "function": {
			const arg = node.arg;
			
			// const oldResolve = context.resolve;
			// context.resolve = true;
			const argumentType = evaluate(context, arg.type);
			// context.resolve = oldResolve;
			
			let value: ASTnode = {
				kind: "unknowable",
				location: arg.location,
			};
			if (ASTnode_isAtype(argumentType)) {
				value = {
					kind: "_selfType",
					location: arg.location,
					id: "TODO?",
					type: argumentType,
				};
			}
			context.levels[context.levels.length-1].push({
				kind: "alias",
				location: arg.location,
				unalias: false,
				name: arg.name,
				value: value,
			});
			
			const oldSetUnalias = context.setUnalias;
			const oldResolve = context.resolve;
			context.setUnalias = false;
			context.resolve = false;
			const codeBlock = evaluate(context, node.body);
			context.setUnalias = oldSetUnalias;
			context.resolve = oldResolve;
			
			const newFunction: ASTnode_function = {
				kind: "function",
				location: node.location,
				hasError: false,
				arg: arg,
				body: codeBlock,
			};
			
			logger.log(LogType.evaluate, "newFunction", printASTnode({level: 0}, newFunction));
			
			return newFunction;
		}
		
		case "if": {
			const condition = evaluate(context, node.condition);
			if (condition.kind != "bool") {
				throw utilities.unreachable();
			}
			
			if (condition.value) {
				const result = evaluate(context, node.trueBody);
				return result;
			} else {
				const result = evaluate(context, node.falseBody);
				return result;
			}
		}
		
		case "alias": {
			const value = evaluate(context, node.value);
			
			return {
				kind: "alias",
				location: node.location,
				unalias: node.unalias,
				name: node.name,
				value: value,
			};
		}
	}
	
	return node;
}

export function evaluateList(context: BuilderContext, AST: ASTnode[]): ASTnode[] {
	context.levels.push([]);
	
	let output = [];
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		const result = evaluate(context, ASTnode);
		// if (result.kind == "alias") {
		// 	if (context.setUnalias) {
		// 		result.unalias = true;
		// 	}
		// 	context.levels[context.levels.length-1].push(result);
		// }
		// if (ASTnode.kind == "identifier") {
		// 	const alias = getAlias(context, ASTnode.name);
		// 	if (alias) {
		// 		if (alias.unalias) {
		// 			logger.log(LogType.evaluate, `unalias ${ASTnode.name}`);
		// 			output.push(AST[i]);
		// 			continue;
		// 		} else {
		// 			logger.log(LogType.evaluate, `no unalias ${ASTnode.name}`);
		// 		}
		// 	}
		// }
		output.push(result);
	}
	
	context.levels.pop();
	
	return output;
}