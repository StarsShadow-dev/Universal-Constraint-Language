import { createHash } from "crypto";

import { ASTnode } from "./parser";
import utilities from "./utilities";

type TreeHash = string;

type topLevelDef = {
	hash: TreeHash,
	ir: IRNode[],
};

type genericIRNode = {
	// location: SourceLocation,
};

export type IRNode = 
genericIRNode & {
	kind: "bool",
	value: boolean,
} |
genericIRNode & {
	kind: "number",
	value: number,
} |
genericIRNode & {
	kind: "string",
	value: string,
};

function hashString(text: string): TreeHash {
	return createHash("sha256").update(text).digest("hex");
	// return createHash("sha512").update(text).digest("hex");
}

function ASTtoIR(ASTnode: ASTnode): IRNode {
	switch (ASTnode.kind) {
		case "bool": {
			return {
				kind: "bool",
				value: ASTnode.value,
			};
		}
		case "number": {
			return {
				kind: "number",
				value: ASTnode.value,
			};
		}
		case "string": {
			return {
				kind: "string",
				value: ASTnode.value,
			};
		}
	
		default: {
			throw utilities.TODO();
		}
	}
}

function getIR(AST: ASTnode[]): IRNode[] {
	let IRNodes: IRNode[] = [];
	for (let i = 0; i < AST.length; i++) {
		IRNodes.push(ASTtoIR(AST[i]));
	}
	return IRNodes;
}

export function useAST(AST: ASTnode[]): topLevelDef[] {
	let defList: topLevelDef[] = [];
	
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		
		if (ASTnode.kind == "alias") {
			const value = ASTnode.value;
			switch (value.kind) {
				case "function": {
					const codeBlockIR = getIR(value.codeBlock);
					const text = JSON.stringify(codeBlockIR);
					defList.push({
						hash: hashString(text),
						ir: codeBlockIR,
					});
					break;
				}
			}
		}
	}
	
	return defList;
}