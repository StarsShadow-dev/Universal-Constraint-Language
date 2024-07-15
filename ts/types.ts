export type SourceLocation = "builtin" | {
	path: string,
	line: number,
	startColumn: number,
	endColumn: number,
};