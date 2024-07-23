import utilities from "./utilities";
import { BuilderContext, getAlias, unAlias } from "./db";
import { ASTnode, ASTnode_argument, ASTnode_function, ASTnode_isAtype } from "./parser";
import { evaluateBuiltin } from "./builtin";
import logger, { LogType } from "./logger";
import { printASTnode } from "./printAST";

export function evaluateNode(context: BuilderContext, node: ASTnode): ASTnode {
	switch (node.kind) {
		case "identifier": {
			const value = unAlias(context, node.name);
			if (!value) throw utilities.unreachable();
			
			if (context.resolve) {
				return evaluateNode(context, value);
			} else {
				const alias = getAlias(context, node.name);
				if (alias && alias.unalias) {
					return evaluateNode(context, value);
				}
				return node;
			}
		}
		
		case "call": {
			const functionToCall = evaluate(context, [node.left])[0];
			if (functionToCall.kind != "function") {
				throw utilities.TODO();
			}
			
			const oldSetUnalias = context.setUnalias;
			context.levels.push([]);
			context.setUnalias = true;
			for (let i = 0; i < node.callArguments.length; i++) {
				const arg = functionToCall.functionArguments[i];
				const argValue = node.callArguments[i];
				context.levels[context.levels.length-1].push({
					kind: "alias",
					location: arg.location,
					unalias: true,
					name: arg.name,
					value: argValue,
				});
			}
			const resultList = evaluate(context, functionToCall.codeBlock);
			const result = resultList[resultList.length-1];
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
				if (node.left.kind != "number" || node.right.kind != "number") {
					throw utilities.unreachable();
				}
				const left = node.left.value;
				const right = node.right.value;
				
				let result: number;
				if (op == "+") {
					result = left + right;
				} else if (op == "-") {
					result = left - right;
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
			debugger;
			
			let functionArguments: ASTnode_argument[] = [];
			for (let i = 0; i < node.functionArguments.length; i++) {
				const functionArgument = node.functionArguments[i];
				
				// const oldResolve = context.resolve;
				// context.resolve = true;
				const argumentType = evaluateNode(context, functionArgument.type);
				// context.resolve = oldResolve;
				
				functionArguments.push({
					kind: "argument",
					location: functionArgument.location,
					name: functionArgument.name,
					type: argumentType,
				});
				
				let value: ASTnode = {
					kind: "unknowable",
					location: functionArgument.location,
				};
				if (ASTnode_isAtype(argumentType)) {
					value = {
						kind: "_selfType",
						location: functionArgument.location,
						id: "TODO?",
						type: argumentType,
					};
				}
				context.levels[context.levels.length-1].push({
					kind: "alias",
					location: functionArgument.location,
					unalias: false,
					name: functionArgument.name,
					value: value,
				});
			}
			
			const returnType = evaluateNode(context, node.returnType);
			
			const oldSetUnalias = context.setUnalias;
			const oldResolve = context.resolve;
			context.setUnalias = false;
			context.resolve = false;
			const codeBlock = evaluate(context, node.codeBlock);
			context.setUnalias = oldSetUnalias;
			context.resolve = oldResolve;
			
			const newFunction: ASTnode_function = {
				kind: "function",
				location: node.location,
				hasError: false,
				functionArguments: functionArguments,
				returnType: returnType,
				codeBlock: codeBlock,
			};
			
			// logger.log(LogType.evaluate, "newFunction", printASTnode({level: 0}, newFunction));
			
			return newFunction;
		}
		
		case "if": {
			const condition = evaluateNode(context, node.condition);
			if (condition.kind != "bool") {
				throw utilities.unreachable();
			}
			
			if (condition.value) {
				const resultList = evaluate(context, node.trueCodeBlock);
				const result = resultList[resultList.length-1];
				return result;
			} else {
				const resultList = evaluate(context, node.falseCodeBlock);
				const result = resultList[resultList.length-1];
				return result;
			}
		}
		
		case "alias": {
			const value = evaluateNode(context, node.value);
			
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

export function evaluate(context: BuilderContext, AST: ASTnode[]): ASTnode[] {
	context.levels.push([]);
	
	let output = [];
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		const result = evaluateNode(context, ASTnode);
		if (result.kind == "alias") {
			if (context.setUnalias) {
				result.unalias = true;
			}
			context.levels[context.levels.length-1].push(result);
		}
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