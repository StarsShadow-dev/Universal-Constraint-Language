//
// general
//

import { CompileError } from "./report";
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
	comptime: boolean,
	mutable: boolean,
	isAfield: boolean,
	name: string,
	type: ASTnode & { kind: "typeUse" } | null,
	value: ASTnode | null,
} | genericASTnode & {
	kind: "function",
	forceInline: boolean,
	functionArguments: ASTnode[],
	returnType: ASTnode & { kind: "typeUse" } | null,
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "struct",
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
	kind: "return",
	value: ASTnode | null,
} | genericASTnode & {
	kind: "argument",
	name: string,
	type: ASTnode & { kind: "typeUse" },
} | genericASTnode & {
	kind: "structInstance",
	templateStruct: ASTnode & { kind: "typeUse" },
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "setInstanceField",
	name: string,
	value: ASTnode,
} | genericASTnode & {
	kind: "typeUse",
	value: ASTnode,
}

//
// scope
//

type GenericScopeObject = {
	originLocation: SourceLocation,
}

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
} | GenericScopeObject & {
	kind: "complexValue",
	type: (ScopeObject & { kind: "typeUse" }),
} | GenericScopeObject & {
	kind: "alias",
	mutable: boolean,
	isAfield: boolean,
	name: string,
	value: ScopeObject | null,
	symbolName: string,
	type: (ScopeObject & { kind: "typeUse" }) | null,
} | GenericScopeObject & {
	kind: "function",
	forceInline: boolean,
	external: boolean,
	toBeGenerated: boolean,
	indentation: number,
	symbolName: string,
	functionArguments: (ScopeObject & { kind: "argument" })[],
	returnType: (ScopeObject & { kind: "typeUse" }) | null,
	AST: ASTnode[],
	visible: ScopeObject[],
} | GenericScopeObject & {
	kind: "argument",
	name: string,
	type: (ScopeObject & { kind: "typeUse" }),
} | GenericScopeObject & {
	kind: "struct",
	name: string,
	members: (ScopeObject & { kind: "alias" })[],
} | GenericScopeObject & {
	kind: "structInstance",
	templateStruct: (ScopeObject & { kind: "struct" }),
	fields: (ScopeObject & { kind: "alias" })[],
} | GenericScopeObject & {
	kind: "typeUse",
	type: ScopeObject,
}

export function unwrapScopeObject(scopeObject: ScopeObject | null): ScopeObject {
	if (scopeObject) {
		if (scopeObject.kind == "alias") {
			if (scopeObject.value) {
				return scopeObject.value;
			} else {
				throw new CompileError(`alias '${scopeObject.name}' used before its definition`)
					.indicator(scopeObject.originLocation, "alias defined here");
			}
		}
		
		return scopeObject;	
	} else {
		throw utilities.unreachable();
	}
}

export function getTypeName(typeUse: ScopeObject & { kind: "typeUse" }): string {
	if (typeUse.type.kind == "struct") {
		return typeUse.type.name;
	} else {
		throw utilities.unreachable();
	}
}