export type IdeOptions = {
	mode: "compileFile",
};

export type CompilerOptions = {
	filePath: string,
	fancyErrors: boolean,
	includeLogs: string[],
	
	outputPath?: string,
	ideOptions?: IdeOptions,
};