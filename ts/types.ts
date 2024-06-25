import utilities from "./utilities";

export type SourceLocation = "builtin" | {
	path: string,
	line: number,
	startColumn: number,
	endColumn: number,
};
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

export type OpCode_builtinCall = genericOpCode & {
	kind: "builtinCall",
	name: string,
	callArguments: OpCode[],
};

export type OpCode_argument = genericOpCode & {
	kind: "argument",
	name: string,
	type: OpCode,
};

type genericOpCodeType = genericOpCode & {
	id: string,
	haveSetId: boolean,
};

export type OpCodeType =
genericOpCodeType & {
	kind: "struct",
	caseTag: number | null,
	doCheck: boolean,
	fields: OpCode_argument[],
	codeBlock: OpCode[],
} | genericOpCodeType & {
	kind: "enum",
	doCheck: boolean,
	codeBlock: OpCode[],
} | genericOpCodeType & {
	kind: "functionType",
	functionArguments: OpCode[],
	returnType: OpCode,
};
export function OpCode_isAtype(opCode: OpCode): opCode is OpCodeType {
	if (opCode.kind == "struct" || opCode.kind == "enum" || opCode.kind == "functionType") {
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
	if (opCode.kind == "functionType") {
		if (!OpCode_isAtype(opCode.returnType)) {
			throw utilities.unreachable();
		}
		return `\\() -> ${OpCodeType_getName(opCode.returnType)}`;
	}
	return `${opCode.id}`;
}

export type OpCode_function = genericOpCode & {
	kind: "function",
	id: string,
	haveSetId: boolean,
	doTransformations: boolean,
	forceInline: boolean,
	functionArguments: OpCode_argument[],
	returnType: OpCode,
	codeBlock: OpCode[],
};

export type OpCode_alias = genericOpCode & {
	kind: "alias",
	disableGeneration: boolean,
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
} |
OpCode_builtinCall |
genericOpCode & {
	kind: "operator",
	operatorText: string,
	left: OpCode,
	right: OpCode,
} | genericOpCode & {
	kind: "field",
	name: string,
	type: OpCode,
} |
OpCode_argument |
OpCodeType |
OpCode_function |
genericOpCode & {
	kind: "return",
	expression: OpCode,
} |
genericOpCode & {
	kind: "match",
	expression: OpCode,
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
	codeBlock: OpCode[],
} | genericOpCode & {
	kind: "instance",
	caseTag: number | null,
	template: OpCode,
	codeBlock: OpCode[],
} |
OpCode_alias;