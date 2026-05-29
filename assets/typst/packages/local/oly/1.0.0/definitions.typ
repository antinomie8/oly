#let fonts = (
	text: ("Libertinus Serif", "Noto Serif CJK TC", "Noto Color Emoji"),
	sans: ("Noto Sans", "Noto Sans CJK TC", "Noto Color Emoji"),
	mono: "Inconsolata",
)

#let colors = (
	title: eastern,
	headers: rgb("#bf0040"),
	partfill: rgb("#002299"),
	label: rgb("#7f0000"),
	hyperlink: blue,
	strong: rgb("#000055"),
	env: (
		theorems: (
			accent: rgb("#5787a0"),
			stroke: rgb("#0000ff"),
			fill: rgb("#f1f9fa"),
		),
		examples: (
			accent: rgb("#964108"),
			stroke: rgb("#964108") + 0.5pt,
			fill: rgb("#fcf8f8"),
		),
	),
)

// language
#let env_names = (
	"fr": (
		"theorem": "Théorème",
		"corollary": "Corollaire",
		"lemma": "Lemme",
		"proof": "Preuve",
		"proposition": "Proposition",
		"definition": "Définition",
		"notation": "Notation",
		"exercise": "Exercice",
		"example": "Exemple",
		"remark": "Remarque",
		"solution": "Solution",
		"conjecture": "Conjecture",
		"problem": "Problème",
		"algorithm": "Algorithme",
    "reformulation": "Reformulation",
		"toc": "Table des matières",
	),
	"en": (
		"theorem": "Theorem",
		"corollary": "Corollary",
		"lemma": "Lemma",
		"proof": "Proof",
		"proposition": "Proposition",
		"definition": "Definition",
		"notation": "Notation",
		"exercise": "Exercise",
		"example": "Example",
		"remark": "Remark",
		"solution": "Solution",
		"conjecture": "Conjecture",
		"problem": "Problem",
    "algorithm": "Algorithm",
    "reformulation": "Reformulation",
		"toc": "Table of contents",
	),
)
#let get_env_name(env) = {
	return context env_names.at(
    text.lang,
    default: env_names.at("en"),
  ).at(
    env,
    default: upper(env.at(0)) + env.slice(1) // capitalize first letter
  )
}

#let months = (
	"en": (
		"January": "January",
		"February": "February",
		"March": "March",
		"April": "April",
		"May": "May",
		"June": "June",
		"July": "July",
		"August": "August",
		"September": "September",
		"October": "October",
		"November": "November",
		"December": "December",
	),
	"fr": (
		"January": "Janvier",
		"February": "Février",
		"March": "Mars",
		"April": "Avril",
		"May": "Mai",
		"June": "Juin",
		"July": "Juillet",
		"August": "Août",
		"September": "Septembre",
		"October": "Octobre",
		"November": "Novembre",
		"December": "Décembre",
	),
)
