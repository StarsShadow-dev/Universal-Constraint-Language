//
// general
//

export type SourceLocation = {
	path: string,
	line: number,
	startColumn: number,
	endColumn: number,
} | "builtin"

export type CodeGenText = [string] | null

export function getCGText(): [string] {
	return [""];
}

//
// tokens
//

export enum TokenType {
	comment,
	
	number,
	string,
	word,
	
	separator,
	operator,
	
	builtinIndicator,
	selfReference,
}

export type Token = {
	type: TokenType,
	text: string,
	location: SourceLocation,
	endLine?: number,
}

//
// AST
//

type genericASTnode = {
	location: SourceLocation,
}

export type ASTnode = 

//
// literals
//

genericASTnode & {
	kind: "bool",
	value: boolean,
} | genericASTnode & {
	kind: "number",
	value: number,
} | genericASTnode & {
	kind: "string",
	value: string,
} |

genericASTnode & {
	kind: "identifier",
	name: string,
} | genericASTnode & {
	kind: "call",
	left: ASTnode[],
	callArguments: ASTnode[],
} | genericASTnode & {
	kind: "builtinCall",
	name: string,
	callArguments: ASTnode[],
} | genericASTnode & {
	kind: "operator",
	operatorText: string,
	left: ASTnode[],
	right: ASTnode[],
} |

//
// structured things (what are these called?)
//

genericASTnode & {
	kind: "definition",
	mutable: boolean,
	name: string,
	value: ASTnode,
} | genericASTnode & {
	kind: "function",
	functionArguments: ASTnode[],
	returnType: ASTnode & { kind: "typeUse" } | null,
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "struct",
} | genericASTnode & {
	kind: "codeGenerate",
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "while",
	condition: ASTnode[],
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "if",
	condition: ASTnode[],
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "return",
	value: ASTnode[],
} | genericASTnode & {
	kind: "argument",
	name: string,
	type: ASTnode & { kind: "typeUse" },
} | genericASTnode & {
	kind: "typeUse",
	value: ASTnode,
}

//
// scope
//

type genericScopeObject = {
	originLocation: SourceLocation,
}

export type ScopeObject = genericScopeObject & {
	kind: "bool",
	value: boolean,
} | genericScopeObject & {
	kind: "number",
	value: number,
} | genericScopeObject & {
	kind: "string",
	value: string,
} | genericScopeObject & {
	kind: "void",
} | genericScopeObject & {
	kind: "complexValue",
	type: ScopeObject[],
} | genericScopeObject & {
	kind: "alias",
	mutable: boolean,
	name: string,
	value: ScopeObject | null,
	symbolName: string,
} | genericScopeObject & {
	kind: "function",
	symbolName: string,
	functionArguments: ScopeObject[],
	returnType: ScopeObject[] | null,
	AST: ASTnode[],
	visible: ScopeObject[],
} | genericScopeObject & {
	kind: "struct",
	properties: ScopeObject[],
} | genericScopeObject & {
	kind: "type",
	name: string,
} | genericScopeObject & {
	kind: "argument",
	name: string,
	type: ScopeObject[],
}

export function unwrapScopeObject(scopeObject: ScopeObject): ScopeObject {
	if (scopeObject.kind == "alias" && scopeObject.value) {
		return scopeObject.value;
	}
	
	return scopeObject;
}