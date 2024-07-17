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