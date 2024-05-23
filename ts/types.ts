//
// general
//

import utilities from "./utilities";

export type SourceLocation = {
	path: string,
	line: number,
	startColumn: number,
	endColumn: number,
} | "builtin"

export type CodeGenText = string[] | null

export function getCGText(): string[] {
	return [];
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
	singleQuote,
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

export type ASTnode = genericASTnode & {
	kind: "bool",
	value: boolean,
} | genericASTnode & {
	kind: "number",
	value: number,
} | genericASTnode & {
	kind: "string",
	value: string,
} | genericASTnode & {
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
} | genericASTnode & {
	kind: "definition",
	name: string,
	value: ASTnode,
} | genericASTnode & {
	kind: "field",
	name: string,
	type: ASTnode,
} | genericASTnode & {
	kind: "function",
	forceInline: boolean,
	functionArguments: ASTnode[],
	returnType: ASTnode & { kind: "typeUse" } | null,
	comptimeReturn: boolean,
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "struct",
	fields: ASTnode[],
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "while",
	condition: ASTnode[],
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "if",
	condition: ASTnode[],
	trueCodeBlock: ASTnode[],
	falseCodeBlock: ASTnode[] | null,
} | genericASTnode & {
	kind: "codeBlock",
	comptime: boolean,
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "argument",
	comptime: boolean,
	name: string,
	type: ASTnode & { kind: "typeUse" },
} | genericASTnode & {
	kind: "structInstance",
	templateStruct: ASTnode & { kind: "typeUse" },
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "typeUse",
	value: ASTnode,
}

//
// scope
//

type GenericScopeObject = {
	originLocation: SourceLocation,
};

export type ScopeObject_alias = GenericScopeObject & {
	kind: "alias",
	isAfield: boolean,
	name: string,
	symbolName: string,
	value: ScopeObject,
	valueAST: ASTnode | null,
};

export type ScopeObjectType = ScopeObject_alias & {
	value: ScopeObject_function | ScopeObject_struct,
};
export function isScopeObjectType(scopeObject: ScopeObject): scopeObject is ScopeObjectType {
	if (scopeObject.kind != "alias" || !scopeObject.value) {
		return false;
	}
	if (scopeObject.value.kind != "function" && scopeObject.value.kind != "struct") {
		return false;
	}
	return true;
}
export function cast_ScopeObjectType(scopeObject: ScopeObject | null): ScopeObjectType {
	if (!scopeObject || !isScopeObjectType(scopeObject)) {
		throw utilities.unreachable();
	}
	
	return scopeObject;
}

export type ScopeObject_function = GenericScopeObject & {
	kind: "function",
	forceInline: boolean,
	external: boolean,
	toBeGenerated: boolean,
	toBeChecked: boolean,
	hadError: boolean,
	indentation: number,
	symbolName: string,
	functionArguments: (ScopeObject & { kind: "argument" })[],
	returnType: ScopeObjectType | null,
	comptimeReturn: boolean,
	AST: ASTnode[],
	visible: ScopeObject_alias[],
};

export type ScopeObject_struct = GenericScopeObject & {
	kind: "struct",
	name: string,
	toBeChecked: boolean,
	fields: ScopeObject_argument[],
	members: ScopeObject_alias[],
};

export type ScopeObject_argument = GenericScopeObject & {
	kind: "argument",
	comptime: boolean,
	name: string,
	type: ScopeObjectType,
};

export type ScopeObject_complexValue = GenericScopeObject & {
	kind: "complexValue",
	type: ScopeObjectType,
};

export type ScopeObject = GenericScopeObject & {
	kind: "bool",
	value: boolean,
} | GenericScopeObject & {
	kind: "number",
	value: number,
} | GenericScopeObject & {
	kind: "string",
	value: string,
} | GenericScopeObject & {
	kind: "void",
} | 
ScopeObject_complexValue |
ScopeObject_alias |
ScopeObject_function | 
ScopeObject_argument | 
ScopeObject_struct |
GenericScopeObject & {
	kind: "structInstance",
	templateStruct: (ScopeObject & { kind: "struct" }),
	fields: ScopeObject_alias[],
};

export function unwrapScopeObject(scopeObject: ScopeObject | null): ScopeObject {
	if (!scopeObject) {
		throw utilities.unreachable();
	}
	
	if (scopeObject.kind == "alias") {
		return scopeObject.value;
	}
	
	return scopeObject;	
}

export function getTypeName(type: ScopeObjectType): string {
	return type.name;
}