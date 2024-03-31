import * as fs from 'fs';
import { exit } from "process";

export default {
	unreachable(): never {
		debugger;
		console.error("unreachable code reached");
		exit(1);
	},
	
	readFile(path: string): string {
		const text = fs.readFileSync(path, { encoding: 'utf8' });
		return text;
	}
}