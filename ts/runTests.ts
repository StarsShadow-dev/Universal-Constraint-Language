import * as fs from "fs";
import path from "path";

import utilities from "./utilities";
import logger from "./logger";
import { CompilerOptions } from "./compiler";
import { addToDB, makeDB } from "./db";
import { lex, TokenKind } from "./lexer";
import { getIndicatorText } from "./report";
import { parse, ParserMode } from "./parser";

const c_green = "\x1B[32m";
const c_red = "\x1B[31m"
const c_reset = "\x1B[0m";

let total = 0;
let skipped = 0;
let succeeded = 0;
let failed = 0;

function testSkip() {
	console.log(`\n\t\x1B[34mTest skipped.${c_reset}\n`);
	skipped++;
}

function testSuccess() {
	console.log(`\n\t${c_green}Test success!${c_reset}\n`);
	succeeded++;
}

function testFailure(msg: any) {
	console.log(`\n\t${c_red}Test failure:\n${msg}${c_reset}\n`);
	failed++;
	
	process.exitCode = 1;
}

function testFile(filePath: string) {
	filePath = path.normalize(filePath);
	
	total++;
	
	console.log(`running test: '${filePath}'`);
	
	let comments: any;
	let mode: any;
	
	const options: CompilerOptions = {
		filePath: filePath,
		fancyErrors: false,
	};
	
	try {
		const db = makeDB();

		const text = utilities.readFile(options.filePath);

		const lexStart = Date.now();
		const tokens = lex(options.filePath, text);
		if (!tokens[0] || tokens[0].type != TokenKind.comment) {
			console.log(`unable to test file '${filePath}' (no top comment)`);
			utilities.unreachable();
		}
		
		comments = tokens[0].text.split("\n");
		mode = comments.shift();
		if (mode == "testSkip") {
			throw `__testSkip__`;
		}
		if (mode != "compError" && mode != "compSucceed" && mode != "compOut") {
			throw `unknown mode "${mode}"`;
		}
		logger.addTime("lexing", Date.now() - lexStart);

		const parseStart = Date.now();
		const AST = parse({
			tokens: tokens,
			i: 0,
		}, ParserMode.normal, null);
		logger.addTime("parsing", Date.now() - parseStart);

		addToDB(db, AST);

		if (db.errors.length == 0) {
			if (mode == "compError") {
				const expectedOutput = comments.join("\n");
				testFailure(`expected error output = ${expectedOutput}\n\nactual output is success`);
			} else if (mode == "compSucceed") {
				let output = "";
				for (let i = 0; i < db.topLevelEvaluations.length; i++) {
					const evaluation = db.topLevelEvaluations[i];
					if (evaluation.msg != "true") {
						output += getIndicatorText(evaluation, options.fancyErrors);
					}
				}
				if (output.length == 0) {
					testSuccess();
				} else {
					testFailure(output);
				}
			} else if (mode == "compOut") {
				let actualOutput = "";
				for (let i = 0; i < db.topLevelEvaluations.length; i++) {
					const evaluation = db.topLevelEvaluations[i];
					if (actualOutput != "") {
						actualOutput += "\n";
					}
					actualOutput += getIndicatorText(evaluation, options.fancyErrors);
				}
				
				const expectedOutput = comments.join("\n");
				if (expectedOutput == actualOutput) {
					testSuccess();
				} else {
					testFailure(`-- expectedOutput\n${expectedOutput}\n-- actualOutput\n${actualOutput}`);
				}
			}
		} else {
			let errorText = "";
			for (let i = 0; i < db.errors.length; i++) {
				const error = db.errors[i];
				if (errorText != "") {
					errorText += "\n";
				}
				errorText += error.getText(false);
			}
			
			if (mode == "compSucceed") {
				testFailure(`compilation failed, output = ${errorText}`);
			} else if (mode == "compError") {
				const expectedOutput = comments.join("\n");
				const actualOutput = errorText;
				if (expectedOutput == actualOutput) {
					testSuccess();
				} else {
					testFailure(`-- expectedOutput\n${expectedOutput}\n-- actualOutput\n${actualOutput}`);
				}
			} else if (mode == "compOut") {
				testFailure(`compilation failed, output = ${errorText}`);
			}
		}
	} catch (error) {
		if (error == "__testSkip__") {
			testSkip();
			return;
		}
		
		testFailure(error);
		return;
	}
}

function testDir(dirPath: string) {
	const files = fs.readdirSync(dirPath);
	// console.log(files);
	files.forEach((file) => {
		testFile(path.join(dirPath, file));
	})
}

testDir("./tests/compSucceed");
testDir("./tests/compError");
testDir("./tests/compOut");
// testDir("./tests/js");

console.log(`total: ${total}`);
console.log(`skipped: ${skipped}`);
console.log(`succeeded: ${c_green}${succeeded}${c_reset}`);
console.log(`failed: ${c_red}${failed}${c_reset}`);