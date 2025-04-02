#include "treewidth.hpp"

#include <chrono>
#include <iostream>
#include <fstream>
#include <ostream>
#include <cstdlib>
#include <cassert>
#include <queue>
#include <algorithm>
#include <sstream>

#include "utils.hpp"

extern std::string TW_BINARY;

namespace sspp {

namespace decomp {

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

TreeDecomposition run_flow_cutter(double time, const Graph& graph) {
  int in[2];
  int out[2];
  int pid;
  TreeDecomposition dec(0, 0);

  if (pipe(in) < 0) {
    perror("allocating pipe for child input redirect");
    exit(errno);
  }
  if (pipe(out) < 0) {
    perror("allocating pipe for child output redirect");
    exit(errno);
  }

  pid = fork();
  if (pid == 0) { // child
    if (dup2(in[0], STDIN_FILENO) == -1) {
      exit(errno);
    }
    if (dup2(out[1], STDOUT_FILENO) == -1) {
      exit(errno);
    }

    close(in[0]);
    close(in[1]);
    close(out[0]);
    close(out[1]);

    std::string timeout = to_string(time) + "s";
    cout << "c o CMD: " << "timeout" << " " << timeout << " " << TW_BINARY << endl;
    int res = execlp("timeout", "timeout", timeout.c_str(), TW_BINARY.c_str(), NULL);
    if (res == -1) {
      perror("executing timeout flowcutter");
      exit(errno);
    }
  } else if (pid > 0) { // parent
    close(in[0]);
    close(out[1]);

    auto es = graph.Edges();
    int m = es.size();

    FILE *childin = fdopen(in[1], "w");
    fprintf(childin, "p tw %d %d\n", graph.n(), m);
    for (auto e : es) {
      fprintf(childin, "%d %d\n", e.F+1, e.S+1);
    }
    fflush(childin);
    fclose(childin);
    close(in[1]);
    cout << "c o Primal edges " << es.size() << endl;

    int status;
    waitpid(pid, &status, 0);
    assert(status >= 0);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 124); // TIMEOUT timed out

    cout << "c o tw finish ok" << endl;
    FILE *childout = fdopen(out[0], "r");
    int claim_width = 0;
    char *line = NULL;
    size_t size = 0;
    ssize_t nread;
    while ((nread = getline(&line, &size, childout)) != -1) {
        std::string tmp(line, size);
        std::stringstream ss(tmp);
        ss>>tmp;
        if (tmp == "c") continue;
        if (tmp == "s") {
            ss>>tmp;
            assert(tmp == "td");
            int bs,nn;
            ss>>bs>>claim_width>>nn;
            assert(nn == n);
            claim_width--;
            dec = TreeDecomposition(bs, nn);
        } else if (tmp == "b") {
            int bid;
            ss>>bid;
            vector<int> bag;
            int v;
            while (ss>>v) {
                bag.push_back(v-1);
            }
            dec.SetBag(bid, bag);
        } else {
            int a = stoi(tmp);
            int b;
            ss>>b;
            dec.AddEdge(a, b);
        }
    }
    free(line);
    fclose(childout);
    close(out[0]);
  } else {
    perror("forking");
    exit(errno);
  }
  return dec;
}

TreeDecomposition Treedecomp(const Graph& graph, double time) {
	int n = graph.n();
	if (n == 0) {
		TreeDecomposition dec(0, 0);
		return dec;
	}
	if (n == 1) {
		TreeDecomposition dec(1, 1);
		dec.SetBag(1, {0});
		return dec;
	}
	if (time < 0.099) {
		TreeDecomposition dec(1, n);
		vector<int> all;
		for (int i = 0; i < n; i++) {
			all.push_back(i);
		}
		dec.SetBag(1, all);
		return dec;
	}
	assert(n >= 2);

	TreeDecomposition dec = run_flow_cutter(time, graph);
	assert(dec.Width() <= claim_width);
	cout << "c o width " << dec.Width() << endl;
	assert(dec.Verify(graph));
	return dec;
}
} // namespace decomp
} // namespace sspp
