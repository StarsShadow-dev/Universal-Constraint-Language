// run with `tsc -p ./tsconfig.json && node out/main.js test/main.ucl`

// lexer -> parser -> DB -> evaluate/printAST

import path from "path";

import utilities from "./utilities";
import logger from "./logger";
import { lex } from "./lexer";
import { parse, ParserMode } from "./parser";
import { addToDB, DB, makeDB } from "./db";
import { getIndicatorText } from "./report";
import { CompilerOptions } from "./compiler";

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

export type IdeOptions = {
	mode: "compileFile",
};

const options: CompilerOptions = {
	filePath: next(),
	fancyErrors: true,
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

const db = makeDB();

const text = utilities.readFile(options.filePath);
// console.log("text:", text);

const lexStart = Date.now();
const tokens = lex(options.filePath, text);
logger.addTime("lexing", Date.now() - lexStart);
// console.log("tokens:", tokens);

const parseStart = Date.now();
const AST = parse({
	tokens: tokens,
	i: 0,
}, ParserMode.normal, null);
logger.addTime("parsing", Date.now() - parseStart);
// console.log("AST:", JSON.stringify(AST, null, 2));

addToDB(db, AST);
// console.log("db:", JSON.stringify(db, null, 4));

for (let i = 0; i < db.errors.length; i++) {
	const error = db.errors[i];
	console.log(error.getText(options.fancyErrors));
}

if (db.errors.length == 0) {
	console.log("top level evaluations:");
	for (let i = 0; i < db.topLevelEvaluations.length; i++) {
		const evaluation = db.topLevelEvaluations[i];
		console.log(getIndicatorText(evaluation, options.fancyErrors));
	}
}

// logger.printFileAccessLogs();
// logger.printTimes();