// tsc -p ./tsconfig.json && node out/main.js test/main.ucl

import utilities from "./utilities";
import logger from "./logger";
import { CompilerOptions, compile, newBuilderContext } from "./compiler";
import { printErrors } from "./report";

let i = 2;

function next(): string {
	return process.argv[i++];
}

const options: CompilerOptions = {
	filePath: next(),
	fancyErrors: true,
	builderTransforms: {
		removeTypes: true,
	}
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
	} else {
		utilities.TODO();
	}
}

logger.global("options:", options);

const context = newBuilderContext(options);

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