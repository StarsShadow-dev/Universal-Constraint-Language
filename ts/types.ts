export type SourceLocation = {
	line: number,
	startColumn: number,
	endColumn: number,
}

export enum TokenType {
	string,
	number,
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