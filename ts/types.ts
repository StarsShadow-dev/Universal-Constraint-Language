//
// general
//

export type SourceLocation = {
	path: string,
	line: number,
	startColumn: number,
	endColumn: number,
} | "core"

//
// tokens
//

export enum TokenType {
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

//
// things you get a value from
//

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
} |

//
// structured things (what are these called?)
//

genericASTnode & {
	kind: "definition",
	mutable: boolean,
	name: string,
	value: ASTnode[],
} | genericASTnode & {
	kind: "assignment",
	left: ASTnode[],
	right: ASTnode[],
} | genericASTnode & {
	kind: "function",
	functionArguments: ASTnode[],
	returnType: ASTnode[] | null,
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "struct",
} | genericASTnode & {
	kind: "while",
} | genericASTnode & {
	kind: "if",
} | genericASTnode & {
	kind: "return",
	value: ASTnode[],
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
	kind: "complexValue",
	// TODO
} | genericScopeObject & {
	kind: "alias",
	mutable: boolean,
	name: string,
	value: ScopeObject[] | null,
} | genericScopeObject & {
	kind: "function",
	name: string,
	returnType: ScopeObject[] | null,
	AST: ASTnode[],
} | genericScopeObject & {
	kind: "struct",
	// TODO
} | genericScopeObject & {
	kind: "type",
	name: string,
}