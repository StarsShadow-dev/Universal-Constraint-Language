import { IdeOptions } from "./main";

export type CompilerOptions = {
	filePath: string,
	fancyErrors: boolean,
	
	outputPath?: string,
	ideOptions?: IdeOptions,
};