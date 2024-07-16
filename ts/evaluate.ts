import utilities from "./utilities";
import { DB, getDefFromList } from "./db";
import { ASTnode } from "./parser";

export type EvaluateContext = {
	db: DB,
};

export function evaluate(context: EvaluateContext, node: ASTnode): ASTnode {
	switch (node.kind) {
		case "identifier": {
			const def = getDefFromList(context.db.defs, node.name);
			if (!def) throw utilities.unreachable();
			return def.ASTnode;
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

export function evaluateList(context: EvaluateContext, AST: ASTnode[]): ASTnode {
	let node = null;
	for (let i = 0; i < AST.length; i++) {
		node = evaluate(context, AST[i]);
	}
	
	if (!node) {
		throw utilities.unreachable();
	}
	return node;
}