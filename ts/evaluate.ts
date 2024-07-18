import utilities from "./utilities";
import { BuilderContext, unAlias } from "./db";
import { ASTnode, ASTnode_alias } from "./parser";
import { builtinTypes } from "./builtin";

export function evaluate(context: BuilderContext, node: ASTnode): ASTnode {
	switch (node.kind) {
		case "identifier": {
			const alias = unAlias(context, node.name);
			if (!alias) throw utilities.unreachable();
			return alias;
		}
		
		case "call": {
			const left = evaluate(context, node.left);
			if (left.kind != "function") {
				throw utilities.TODO();
			}
			const result = evaluateList(context, left.codeBlock);
			return result;
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
			}
		}
	}
	
	return node;
}

export function evaluateList(context: BuilderContext, AST: ASTnode[]): ASTnode {
	let node = null;
	for (let i = 0; i < AST.length; i++) {
		node = evaluate(context, AST[i]);
	}
	
	if (!node) {
		throw utilities.unreachable();
	}
	return node;
}