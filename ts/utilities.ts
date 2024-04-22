import * as fs from "fs";
import { exit } from "process";
import { CompileError } from "./report";

export default {
	unreachable(): never {
		debugger;
		console.error("unreachable code reached");
		exit(1);
	},
	
	TODO(): never {
		debugger;
		console.error("TODO reached");
		exit(1);
	},
	
	readFile(path: string): string {
		try {
			const text = fs.readFileSync(path, { encoding: 'utf8' });
			return text;
		} catch (error) {
			throw new CompileError(`could not read file at path '${path}'`);
		}
	},
}