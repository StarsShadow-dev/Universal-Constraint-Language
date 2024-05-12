// tsc -p ./tsconfig.json && node out/main.js test/main.ucl

import { setUpBuiltin } from "./builtin";
import { CompilerOptions, compile, newBuilderContext } from "./compiler";
import { CompileError, printErrors } from "./report";
import logger from "./logger";
import utilities from "./utilities";

let i = 2;

function next(): string {
	return process.argv[i++];
}

const options: CompilerOptions = {
	filePath: next(),
	check: true,
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
	} else if (arg == "-noCheck") {
		options.check = false;
	} else if (arg == "-noFancyErrors") {
		options.fancyErrors = false;
	} else {
		utilities.TODO();
	}
}

logger.global("options:", options);

const context = newBuilderContext(options);

setUpBuiltin(false);
compile(context, null);
if (context.errors.length == 0) {
	if (options.outputPath) {
		utilities.writeFile(options.outputPath, context.topCodeGenText.join(""));
	} else {
		if (!options.ideOptions) {
			console.log(context.topCodeGenText.join(""));
		}
	}
} else {
	process.exitCode = 1;
	printErrors(options, context.errors);
}

// logger.printFileAccessLogs();
// logger.printTimes();