import utilities from "./utilities";

export type SourceLocation = "builtin" | {
	path: string,
	line: number,
	startColumn: number,
	endColumn: number,
};

export function getCGText(): string[] {
	return [];
}

export enum TokenKind {
	comment,
	
	number,
	string,
	word,
	
	separator,
	operator,
	
	builtinIndicator,
	selfReference,
	singleQuote,
	
	endOfFile,
};

export type Token = {
	type: TokenKind,
	text: string,
	location: SourceLocation,
	endLine?: number,
};

//
// OpCode
//

type genericOpCode = {
	location: SourceLocation,
};

type genericOpCodeType = genericOpCode & {
	id: string,
};

export type OpCodeType = genericOpCodeType & {
	kind: "struct",
	fields: OpCode[],
	codeBlock: OpCode[],
} | genericOpCodeType & {
	kind: "enum",
	codeBlock: OpCode[],
};
export function OpCode_isAtype(opCode: OpCode): opCode is OpCodeType {
	if (opCode.kind == "struct" || opCode.kind == "enum") {
		return true;
	} else {
		return false;
	}
}
export function OpCode_castType(opCode: OpCode): OpCodeType {
	if (OpCode_isAtype(opCode)) {
		return opCode;
	} else {
		throw utilities.unreachable();
	}
}
export function OpCodeType_getName(opCode: OpCodeType): string {
	return `'${opCode.id}'`;
}

export type OpCode_argument = genericOpCode & {
	kind: "argument",
	comptime: boolean,
	name: string,
	type: OpCode,
};

export type OpCode_function = genericOpCode & {
	kind: "function",
	forceInline: boolean,
	functionArguments: OpCode_argument[],
	returnType: OpCode,
	codeBlock: OpCode[],
};

export type OpCode_alias = genericOpCode & {
	kind: "alias",
	name: string,
	value: OpCode,
};

export type OpCode =
genericOpCode & {
	kind: "bool",
	value: boolean,
} | genericOpCode & {
	kind: "number",
	value: number,
} | genericOpCode & {
	kind: "string",
	value: string,
} | genericOpCode & {
	kind: "complexValue",
	type: OpCodeType,
} | genericOpCode & {
	kind: "identifier",
	name: string,
} | genericOpCode & {
	kind: "list",
	elements: OpCode[],
} | genericOpCode & {
	kind: "call",
	left: OpCode,
	callArguments: OpCode[],
} | genericOpCode & {
	kind: "builtinCall",
	name: string,
	callArguments: OpCode[],
} | genericOpCode & {
	kind: "operator",
	operatorText: string,
	left: OpCode[],
	right: OpCode[],
} | genericOpCode & {
	kind: "field",
	name: string,
	type: OpCode,
} |
OpCodeType |
OpCode_argument |
OpCode_function | genericOpCode & {
	kind: "match",
	expression: OpCode, // TODO: name
	codeBlock: OpCode[],
} | genericOpCode & {
	kind: "matchCase",
	name: string,
	types: OpCode[],
	codeBlock: OpCode[],
} | genericOpCode & {
	kind: "while",
	condition: OpCode,
	codeBlock: OpCode[],
} | genericOpCode & {
	kind: "if",
	condition: OpCode,
	trueCodeBlock: OpCode[],
	falseCodeBlock: OpCode[],
} | genericOpCode & {
	kind: "codeBlock",
	comptime: boolean,
	codeBlock: OpCode[],
} | genericOpCode & {
	kind: "newInstance",
	template: OpCode,
	codeBlock: OpCode[],
} |
OpCode_alias;