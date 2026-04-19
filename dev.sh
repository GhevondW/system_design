#!/bin/bash
# SystemForge Development Helper

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_usage() {
    echo "SystemForge Development Helper"
    echo ""
    echo "Usage: ./dev.sh [command]"
    echo ""
    echo "Commands:"
    echo "  test-native       Run native C++ tests"
    echo "  test-wasm         Build WASM target"
    echo "  test-ui           Run web UI unit tests (JSDOM)"
    echo "  test-all          Run all tests (native + wasm + ui)"
    echo "  build             Build native binary"
    echo "  serve             Build WASM and serve web UI (port 8080)"
    echo "  clean             Clean all build artifacts"
    echo "  format            Format all C++ code"
    echo "  shell             Open development shell"
    echo "  wasm-shell        Open shell with Emscripten active"
    echo "  help              Show this help message"
}

run_command() {
    echo -e "${GREEN}Running: $1${NC}"
    docker compose up --build "$2"
}

case "${1:-help}" in
    test-native)
        run_command "Native Tests" "test-native"
        ;;
    test-wasm)
        run_command "WASM Build" "test-wasm"
        ;;
    test-ui)
        run_command "Web UI Tests" "test-ui"
        ;;
    test-all)
        echo -e "${GREEN}Running all tests...${NC}"
        docker compose up --build test-native
        docker compose up --build test-wasm
        docker compose up --build test-ui
        echo -e "${GREEN}All tests passed!${NC}"
        ;;
    build)
        run_command "Native Build" "build-native"
        ;;
    serve)
        echo -e "${GREEN}Building and serving SystemForge...${NC}"
        echo -e "${YELLOW}Building in ${BUILD_TYPE:-Release} mode${NC}"
        echo ""
        echo -e "  ${GREEN}-> http://localhost:8080${NC}"
        echo ""
        echo -e "${YELLOW}Tip: BUILD_TYPE=Debug ./dev.sh serve${NC}"
        docker compose up --build serve
        ;;
    clean)
        echo -e "${YELLOW}Cleaning build artifacts...${NC}"
        rm -rf build/
        docker compose down
        echo -e "${GREEN}Clean complete${NC}"
        ;;
    format)
        echo -e "${GREEN}Formatting code...${NC}"
        docker compose run --rm shell bash -c "find src tests -name '*.cpp' -o -name '*.h' | xargs clang-format -i"
        echo -e "${GREEN}Format complete${NC}"
        ;;
    shell)
        echo -e "${GREEN}Opening development shell...${NC}"
        docker compose run --rm shell
        ;;
    wasm-shell)
        echo -e "${GREEN}Opening WASM development shell...${NC}"
        docker compose run --rm wasm-shell
        ;;
    help)
        print_usage
        ;;
    *)
        echo -e "${RED}Unknown command: $1${NC}"
        echo ""
        print_usage
        exit 1
        ;;
esac
