import utilities from "./utilities";
import { BuilderContext, getAlias, unAlias } from "./db";
import { ASTnode } from "./parser";
import { evaluateBuiltin } from "./builtin";
import logger, { LogType } from "./logger";

function evaluateNode(context: BuilderContext, node: ASTnode): ASTnode {
	switch (node.kind) {
		case "identifier": {
			const value = unAlias(context, node.name);
			if (!value) throw utilities.unreachable();
			return value;
		}
		
		case "call": {
			const left = evaluate(context, [node.left]);
			if (left.kind != "function") {
				throw utilities.TODO();
			}
			
			const oldSetUnalias = context.setUnalias;
			context.levels.push([]);
			context.setUnalias = true;
			for (let i = 0; i < node.callArguments.length; i++) {
				const arg = left.functionArguments[i];
				const argValue = node.callArguments[i];
				context.levels[context.levels.length-1].push({
					kind: "alias",
					location: arg.location,
					unalias: true,
					name: arg.name,
					value: [argValue],
				});
			}
			const result = evaluate(context, left.codeBlock);
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
			const oldSetUnalias = context.setUnalias;
			context.setUnalias = false;
			evaluate(context, node.codeBlock);
			context.setUnalias = oldSetUnalias;
		}
	}
	
	return node;
}

export function evaluate(context: BuilderContext, AST: ASTnode[]): ASTnode {
	context.levels.push([]);
	
	let outNode = null;
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		if (ASTnode.kind == "alias") {
			if (context.setUnalias) {
				ASTnode.unalias = true;
			}
			debugger;
			const value = evaluate(context, ASTnode.value);
			context.levels[context.levels.length-1].push(ASTnode);
			continue;
		}
		outNode = evaluateNode(context, ASTnode);
		if (ASTnode.kind == "identifier") {
			const alias = getAlias(context, ASTnode.name);
			if (alias) {
				if (alias.unalias) {
					logger.log(LogType.evaluate, `unalias ${ASTnode.name}`);
					AST[i] = outNode;
				} else {
					logger.log(LogType.evaluate, `no unalias ${ASTnode.name}`);
				}
			}
		}
	}
	
	context.levels.pop();
	
	if (!outNode) {
		throw utilities.unreachable();
	}
	return outNode;
}