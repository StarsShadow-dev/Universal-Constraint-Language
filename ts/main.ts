// tsc -p ./tsconfig.json && node out/main.js test/main.ucl

import utilities from "./utilities";
import logger from "./logger";

let i = 2;

function next(): string {
	return process.argv[i++];
}

export type IdeOptions = {
	mode: "compileFile",
};

export type CompilerOptions = {
	filePath: string,
	fancyErrors: boolean,
	
	outputPath?: string,
	ideOptions?: IdeOptions,
	dumpOpCodes?: boolean,
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
	} else if (arg == "-d") {
		options.dumpOpCodes = true;
	} else {
		utilities.TODO();
	}
}

logger.log("options", options);

console.log("testing...")

// logger.printFileAccessLogs();
// logger.printTimes();