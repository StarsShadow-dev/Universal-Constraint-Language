import { exit } from "process";

export function unreachable() {
	throw new Error("unreachable code reached");
}