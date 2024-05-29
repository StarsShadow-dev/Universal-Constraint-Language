//
// general
//

import { BuilderContext } from "./compiler";
import { CompileError } from "./report";
import utilities from "./utilities";

export type SourceLocation = "builtin" | {
	path: string,
	line: number,
	startColumn: number,
	endColumn: number,
};

export type CodeGenText = string[] | null;

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
	kind: "list",
	elements: ASTnode[],
} | genericASTnode & {
	kind: "call",
	left: ASTnode,
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
	returnType: ASTnode & { kind: "typeUse" },
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "struct",
	fields: ASTnode[],
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "enum",
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "match",
	expression: ASTnode, // TODO: name
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "matchCase",
	name: string,
	types: ASTnode[],
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "while",
	condition: ASTnode[],
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "if",
	condition: ASTnode[],
	trueCodeBlock: ASTnode[],
	falseCodeBlock: ASTnode[],
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
	kind: "newInstance",
	template: ASTnode & { kind: "typeUse" },
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

type GenericScopeObjectType = {
	id: string,
	preIdType: ScopeObjectType | null,
};

export type ScopeObject_alias = GenericScopeObject & {
	kind: "alias",
	isAfield: boolean,
	name: string,
	symbolName: string,
	value: ScopeObject | null,
	valueAST: ASTnode | null,
};

export type ScopeObjectType = ScopeObject_function |
ScopeObject_struct |
ScopeObject_typeHole |
ScopeObject_enum |
ScopeObject_alias;
export function is_ScopeObjectType(scopeObject: ScopeObject): scopeObject is ScopeObjectType {
	if (
		scopeObject.kind == "function" ||
		scopeObject.kind == "struct" ||
		scopeObject.kind == "typeHole" ||
		scopeObject.kind == "enum" ||
		scopeObject.kind == "alias"
	) {
		if (scopeObject.kind == "alias") {
			return scopeObject.value != null &&
			(is_ScopeObjectType(scopeObject.value) || scopeObject.value.kind == "enumCase");
		}
		return true;
	} else {
		return false;
	}
}
export function cast_ScopeObjectType(scopeObject: ScopeObject | null): ScopeObjectType {
	// if (scopeObject && scopeObject.kind == "alias") {
	// 	scopeObject = unwrapScopeObject(scopeObject);
	// }
	
	if (!scopeObject || !is_ScopeObjectType(scopeObject)) {
		throw utilities.unreachable();
	}
	
	return scopeObject;
}
export function ScopeObjectType_getId(type: ScopeObjectType): string {
	if (type.kind == "alias") {
		const newtype = unwrapScopeObject(type);
		
		if (newtype.kind == "enumCase") {
			if (!type.value || type.value.kind != "enumCase") {
				throw utilities.unreachable();
			}
			const type0 = type.value.types[0];
			if (!type) {
				throw utilities.unreachable();
			}
			return ScopeObjectType_getId(type0);
		}
		
		if (!newtype || !is_ScopeObjectType(newtype)) {
			throw utilities.unreachable();
		}
		
		type = newtype;
	}
	
	if (type.kind == "alias") {
		throw utilities.unreachable();
	}
	
	let id = "";
	if (type.preIdType) {
		id += ScopeObjectType_getId(type.preIdType);
	}
	id += type.id;
	
	return id;
}

export type ScopeObject_function = GenericScopeObject & GenericScopeObjectType & {
	kind: "function",
	forceInline: boolean,
	forceComptime: boolean,
	external: boolean,
	toBeGenerated: boolean,
	toBeChecked: boolean,
	hadError: boolean,
	indentation: number,
	functionArguments: ScopeObject_argument[],
	returnType: ScopeObjectType,
	AST: ASTnode[],
	visible: ScopeObject_alias[],
	implementationOverride: ((context: BuilderContext, args: ScopeObject[]) => ScopeObject) | null,
};

export type ScopeObject_struct = GenericScopeObject & GenericScopeObjectType & {
	kind: "struct",
	toBeChecked: boolean,
	fields: ScopeObject_argument[],
	members: ScopeObject_alias[],
	// conformsTo: ScopeObjectType[],
};

export type ScopeObject_typeHole = GenericScopeObject & GenericScopeObjectType & {
	kind: "typeHole",
};

// TODO: rm this? move to structInstance?
export type ScopeObject_enumCase = GenericScopeObject & {
	kind: "enumCase",
	parent: ScopeObject_enum,
	name: string,
	number: number,
	types: ScopeObjectType[],
};

export type ScopeObject_enum = GenericScopeObject & GenericScopeObjectType & {
	kind: "enum",
	toBeChecked: boolean,
	enumerators: (ScopeObject_alias & { value: ScopeObject_enumCase })[],
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
	kind: "list",
	type: ScopeObjectType,
	elements: ScopeObject[],
} |
ScopeObject_enumCase |
ScopeObject_complexValue |
ScopeObject_alias |
ScopeObject_function | 
ScopeObject_argument | 
ScopeObject_struct |
ScopeObject_typeHole |
ScopeObject_enum |
GenericScopeObject & {
	kind: "structInstance",
	caseName: string | null,
	caseParent: ScopeObject_enum | null,
	template: ScopeObject_struct,
	fields: ScopeObject_alias[],
} | GenericScopeObject & {
	kind: "enumInstance",
	template: ScopeObject_enum,
	caseName: string,
	fields: ScopeObject[],
};

export function unwrapScopeObject(scopeObject: ScopeObject | null, error?: CompileError): ScopeObject {
	if (!scopeObject) {
		if (error != undefined) {
			throw error;
		} else {
			throw utilities.TODO();
		}
	}
	
	if (scopeObject.kind == "alias" && scopeObject.value) {
		return scopeObject.value;
	}
	
	return scopeObject;	
}