// run with `tsc -p ./tsconfig.json && node out/main.js test/main.ucl`

// lexer -> parser -> DB -> evaluate/printAST

import path from "path";

import * as utilities from "./utilities.js";
import logger from "./logger.js";
import { lex } from "./lexer.js";
import { parse, ParserMode } from "./parser.js";
import { DB } from "./db.js";
import { CompileError, getIndicatorText, removeDuplicateErrors } from "./report.js";
import { CompilerOptions } from "./compiler.js";
import { setUpBuiltins } from "./builtin.js";
import { CodeGenContext, printAST } from "./ASTnodes.js";

function readDB(): DB {
	const DBtextPath = path.join(path.dirname(options.filePath), "db.json");
	const DBtext = utilities.readFile(DBtextPath);
	console.log("DBtext:", DBtext);
	const db = JSON.parse(DBtext) as DB;
	console.log("db:", db);
	return db;
}

let i = 2;

function next(): string {
	return process.argv[i++];
}

const options: CompilerOptions = {
	filePath: next(),
	fancyErrors: true,
	includeLogs: [], // TODO
};

while (i < process.argv.length) {
	const arg = next();
	// console.log(`arg: ${arg}`);
	if (arg == "-o") {
		options.outputPath = next();
	} else if (arg == "-ide") {
		const mode = next();
		if (mode == "compileFile") {
			options.ideOptions = {
				mode: mode,
			};
		} else {
			utilities.TODO();
		}
	} else if (arg == "-noFancyErrors") {
		options.fancyErrors = false;
	// } else if (arg == "-d") {
	// 	options.dumpOpCodes = true;
	} else {
		utilities.TODO();
	}
}
// console.log("options:", options);

setUpBuiltins();

const db = new DB();
const text = utilities.readFile(options.filePath);
// console.log("text:", text);

let tokens;
let AST;
try {
	const lexStart = Date.now();
	tokens = lex(options.filePath, text);
	logger.addTime("lexing", Date.now() - lexStart);
	// console.log("tokens:", tokens);

	const parseStart = Date.now();
	AST = parse({
		tokens: tokens,
		i: 0,
	}, ParserMode.normal, 0, null, -1);
	logger.addTime("parsing", Date.now() - parseStart);
	// console.log("AST:", JSON.stringify(AST, null, 2));
	console.log("AST:", printAST(new CodeGenContext(), AST).join("\n"));
	
	db.addAST(AST);
} catch (error) {
	if (error instanceof CompileError) {
		db.errors.push(error);
	} else {
		throw error;
	}
}

console.log("defs:", db.printDefs());

const errors = removeDuplicateErrors(db.errors);

for (let i = 0; i < errors.length; i++) {
	const error = errors[i];
	console.log(error.getText(true, options.fancyErrors));
}

if (errors.length == 0) {
	console.log("top level evaluations:");
	for (let i = 0; i < db.topLevelEvaluations.length; i++) {
		const evaluation = db.topLevelEvaluations[i];
		let indicatorText = getIndicatorText(evaluation, true, options.fancyErrors, evaluation.msg.includes("\n"));
		console.log(indicatorText);
	}
}

// logger.printFileAccessLogs();
// logger.printTimes();