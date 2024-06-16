import utilities from "./utilities";
import logger from "./logger";
import { lex } from "./lexer";
import { CompileError } from "./report";
import {
	ParserMode,
	parse,
} from './parser';
import {
	OpCode,
	OpCode_alias,
	OpCode_function,
	Token,
	getCGText,
} from "./types";
import { getAliasFromList, buildBlock } from "./builder";
import { codeGenList } from "./codeGen";

// TODO: rm?
export enum CompilerStage {
	findAliases = 0,
	evaluateAliasDependencies = 1,
	evaluateAll = 2,
	export = 3,
};

export type IdeOptions = {
	mode: "compileFile",
};

export type BuilderTransforms = {
	removeTypes: boolean,
};

export type CompilerOptions = {
	filePath: string,
	fancyErrors: boolean,
	builderTransforms: BuilderTransforms,
	
	outputPath?: string,
	ideOptions?: IdeOptions,
	dumpOpCodes?: boolean,
};

export type ScopeInformation = {
	levels: OpCode[][],

	function: OpCode_function | null,
	functionArgumentNameText: string,
};

export type FileContext = {
	path: string,
	scope: ScopeInformation,
	opCodes: OpCode[],
};

export type BuilderContext = {
	compilerOptions: CompilerOptions,
	topCodeGenText: string[],
	nextId: number,
	inIndentation: boolean,
	file: FileContext,
	errors: CompileError[],
	
	compilerStage: CompilerStage,
	disableDependencyEvaluation: boolean,
};

// file is null!
export function newBuilderContext(compilerOptions: CompilerOptions): BuilderContext {
	return {
		compilerOptions: compilerOptions,
		topCodeGenText: getCGText(),
		nextId: 0,
		inIndentation: true,
		file: null as any,
		errors: [],
		
		compilerStage: CompilerStage.findAliases,
		disableDependencyEvaluation: false,
	};
}

let filesToCompile: FileContext[] = [];

export function resetFilesToCompile() {
	filesToCompile = [];
}

export function compile(context: BuilderContext, onTokens: null | ((tokens: Token[]) => void)) {
	let mainFile: FileContext;
	try {
		context.compilerStage = CompilerStage.findAliases;
		mainFile = compileFile(context, context.compilerOptions.filePath, onTokens);
		
		// const main = getAlias(mainFile.opCodes, "main");
		// if (main) {
		// 	debugger;
		// }
	} catch (error) {
		if (error instanceof CompileError) {
			context.errors.push(error);
			return;
		} else if (error == "__done__") {
			return;
		} else {
			throw error;
		}
	}
}

export function compileFile(context: BuilderContext, filePath: string, onTokens: null | ((tokens: Token[]) => void)): FileContext {
	// if the file has already been compiled, there is no point compiling it again
	// (this also lets two files import each other)
	for (let i = 0; i < filesToCompile.length; i++) {
		if (filePath == filesToCompile[i].path) {
			return filesToCompile[i];
		}
	}
	
	const oldFile = context.file;
	const newFile: FileContext = {
		path: filePath,
		scope: {
			levels: [],
			function: null,
			functionArgumentNameText: "",
		},
		opCodes: [],
	};
	context.file = newFile;
	filesToCompile.push(newFile);
	
	const text = utilities.readFile(filePath);
	// console.log("text:", text);
	
	const lexStart = Date.now();
	const tokens = lex(filePath, text);
	logger.addTime("lexing", Date.now() - lexStart);
	// console.log("tokens:", tokens);
	
	if (onTokens) {
		onTokens(tokens);
	}
	
	const parseStart = Date.now();
	newFile.opCodes = parse({
		tokens: tokens,
		i: 0,
	}, ParserMode.normal, null);
	logger.addTime("parsing", Date.now() - parseStart);
	// console.log(`OpCodes '${filePath}':`, JSON.stringify(newFile.opCodes, undefined, 4));
	
	buildBlock(context, newFile.opCodes);
	if (context.compilerOptions.dumpOpCodes) {
		console.log(`OpCodes '${filePath}':`, JSON.stringify(newFile.opCodes, undefined, 4));
	}
	
	const js = codeGenList({
		level: 0,
		modes: [],
	}, newFile.opCodes);
	if (context.compilerOptions.dumpOpCodes) {
		console.log(`js '${filePath}':`, js);
	}
	
	context.file = oldFile;
	return newFile;
}