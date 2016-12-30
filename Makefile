default: release

.PHONY: default release debug all clean

include make-utils/flags.mk
include make-utils/cpp-utils.mk

CXX_FLAGS += -pedantic
LD_FLAGS  += -pthread -lopencv_core -lopencv_imgproc -lopencv_highgui

$(eval $(call auto_folder_compile,src))
$(eval $(call auto_add_executable,frakking))

release: release_frakking
release_debug: release_debug_frakking
debug: debug_frakking

all: release release_debug debug

cppcheck:
	cppcheck --force -I include/ --platform=unix64 --suppress=missingIncludeSystem --enable=all --std=c++11 src/*.cpp include/*.hpp

CLANG_FORMAT ?= clang-format-3.7
CLANG_MODERNIZE ?= clang-modernize-3.7
CLANG_TIDY ?= clang-tidy-3.7

format:
	git ls-files "*.hpp" "*.cpp" | xargs ${CLANG_FORMAT} -i -style=file

# Note: This way of doing this is ugly as hell and prevent parallelism, but it seems to be the only way to modernize both sources and headers
modernize:
	git ls-files "*.hpp" "*.cpp" > kws_file_list
	${CLANG_MODERNIZE} -add-override -loop-convert -pass-by-value -use-auto -use-nullptr -p ${PWD} -include-from=kws_file_list
	rm kws_file_list

# clang-tidy with some false positive checks removed
tidy:
	${CLANG_TIDY} -checks='*,-llvm-include-order,-clang-analyzer-alpha.core.PointerArithm,-clang-analyzer-alpha.deadcode.UnreachableCode,-clang-analyzer-alpha.core.IdenticalExpr' -p ${PWD} src/*.cpp -header-filter='include/*' &> tidy_report_light
	echo "The report from clang-tidy is availabe in tidy_report_light"

# clang-tidy with all the checks
tidy_all:
	${CLANG_TIDY} -checks='*' -p ${PWD} src/*.cpp -header-filter='include/*' &> tidy_report_all
	echo "The report from clang-tidy is availabe in tidy_report_all"

clean: base_clean

include make-utils/cpp-utils-finalize.mk
