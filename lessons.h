struct lesson_contents {
	const int text_count;
	const char**const texts;
};
struct lesson {
	const char*const description;
	const char*const source_file_name;
};
struct stage {
	const int lesson_count;
	const char*const name;
	const struct lesson*const lessons;
};

struct stage get_stage(const int index);
struct lesson_contents* load_words_file(const char*const filename);