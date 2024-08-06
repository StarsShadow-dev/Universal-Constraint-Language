import * as utilities from "./utilities";
import { BuilderContext, getAlias, unAlias } from "./db";
import { ASTnode, ASTnode_function, ASTnode_isAtype } from "./parser";
import { evaluateBuiltin } from "./builtin";
import logger, { LogType } from "./logger";
import { printASTnode } from "./printAST";

export function evaluate(context: BuilderContext, node: ASTnode): ASTnode {
	switch (node.kind) {
		case "identifier": {
			const value = unAlias(context, node.name);
			if (!value) throw utilities.unreachable();
			
			if (value.kind == "_selfType") {
				logger.log(LogType.evaluate, `${node.name} = _selfType, can not unAlias`);
				return node;
			}
			
			if (context.resolve) {
				return value;
				// return evaluate(context, value);
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
			const functionToCall = evaluate(context, node.left);
			context.resolve = oldResolve;
			if (functionToCall.kind != "function") {
				throw utilities.TODO();
			}
			
			const oldSetUnalias = context.setUnalias;
			context.setUnalias = true;
			const arg = functionToCall.arg;
			const argValue = evaluate(context, node.arg);
			context.aliases.push({
				kind: "alias",
				location: arg.location,
				unalias: true,
				name: arg.name,
				value: argValue,
			});
			const resultList = evaluateList(context, functionToCall.body);
			const result = resultList[resultList.length-1];
			context.aliases.pop();
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
					// (x + y) knowing x is 1 -> (1 + y)
					return {
						kind: "operator",
						location: node.location,
						operatorText: node.operatorText,
						left: left,
						right: right,
					};
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
			
			context.aliases.push({
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
			const codeBlock = evaluateList(context, node.body);
			context.setUnalias = oldSetUnalias;
			context.resolve = oldResolve;
			context.aliases.pop();
			
			const newFunction: ASTnode_function = {
				kind: "function",
				location: node.location,
				hasError: false,
				arg: {
					kind: "argument",
					location: arg.location,
					name: arg.name,
					type: argumentType,
				},
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
	
	return output;
}