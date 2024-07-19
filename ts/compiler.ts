export type IdeOptions = {
	mode: "compileFile",
};

export type CompilerOptions = {
	filePath: string,
	fancyErrors: boolean,
	
	outputPath?: string,
	ideOptions?: IdeOptions,
};