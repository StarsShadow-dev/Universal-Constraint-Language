import { ASTnode } from "./parser";

type topLevelDef = {
	id: number,
};

type genericIRNode = {
	// location: SourceLocation,
};

export type IRNode = 
genericIRNode & {
	kind: "",
};

export function useAST(AST: ASTnode[]): topLevelDef[] {
	let defList: topLevelDef[] = [];
	
	for (let i = 0; i < AST.length; i++) {
		const node = AST[i];
		defList.push({
			id: 0,
		});
	}
	
	return defList;
}