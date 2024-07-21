import utilities from "./utilities";
import { BuilderContext, unAlias } from "./db";
import { ASTnode } from "./parser";
import { evaluateBuiltin } from "./builtin";

export function evaluateNode(context: BuilderContext, node: ASTnode): ASTnode {
	switch (node.kind) {
		case "identifier": {
			const alias = unAlias(context, node.name);
			if (!alias) throw utilities.unreachable();
			return alias;
		}
		
		case "call": {
			const left = evaluate(context, [node.left]);
			if (left.kind != "function") {
				throw utilities.TODO();
			}
			
			debugger;
			context.levels.push([]);
			for (let i = 0; i < node.callArguments.length; i++) {
				const arg = left.functionArguments[i];
				const argValue = node.callArguments[i];
				context.levels[context.levels.length-1].push({
					kind: "alias",
					location: arg.location,
					name: arg.name,
					value: argValue,
				});
			}
			const result = evaluate(context, left.codeBlock);
			context.levels.pop();
			
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
			evaluate(context, node.codeBlock);
		}
	}
	
	return node;
}

export function evaluate(context: BuilderContext, AST: ASTnode[]): ASTnode {
	let outNode = null;
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		outNode = evaluateNode(context, AST[i]);
		if (ASTnode.kind == "identifier") {
			const value = unAlias(context, ASTnode.name);
			if (!value) {
				throw utilities.unreachable();
			}
			if (value.kind != "_selfType") {
				AST[i] = value;
			}
		}
	}
	
	if (!outNode) {
		throw utilities.unreachable();
	}
	return outNode;
}