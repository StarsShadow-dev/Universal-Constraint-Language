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
} from "./types";
import { buildBlock } from "./builder";
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
	doTransformations: boolean,
	nextId: number,
	file: FileContext,
	errors: CompileError[],
	
	compilerStage: CompilerStage,
	disableDependencyEvaluation: boolean,
	returnEarly: boolean,
	disableImports: boolean,
	outOpCodes: OpCode_alias[],
};

// file is null!
export function newBuilderContext(compilerOptions: CompilerOptions): BuilderContext {
	return {
		compilerOptions: compilerOptions,
		doTransformations: false,
		nextId: 0,
		file: null as any,
		errors: [],
		
		compilerStage: CompilerStage.findAliases,
		disableDependencyEvaluation: false,
		returnEarly: false,
		disableImports: false,
		outOpCodes: [],
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
		
		for (let i = 0; i < filesToCompile.length; i++) {
			const file = filesToCompile[i];
			context.file = file;
			const js = codeGenList({
				level: 0,
				modes: [],
			}, file.opCodes).join("\n");
			if (context.compilerOptions.dumpOpCodes) {
				console.log(`js '${file.path}':\n` + js);
			}
		}
		
		context.file = mainFile;
		
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
			logger.log("file", `file ${filePath} has already been compiled`);
			return filesToCompile[i];
		}
	}
	
	logger.log("file", `compiling file ${filePath}`);
	
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
	if (context.compilerOptions.dumpOpCodes) {
		console.log(`OpCodes '${filePath}':`, JSON.stringify(newFile.opCodes, undefined, 4));
	}
	
	if (newFile.opCodes.length > 0) {
		context.disableImports = true;
		buildBlock(context, newFile.opCodes, false);
		context.disableImports = false;
		buildBlock(context, newFile.opCodes, false);
	}
	
	context.file = oldFile;
	return newFile;
}