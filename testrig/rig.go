package main

import (
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"os/exec"
	"path"
)

func main() {
	if len(os.Args) > 1 && os.Args[1] == "-serve" {
		serve()
	}

	err := build()
	if err != nil {
		os.Exit(1)
	}

	go serve()

	err = runTest()
	if err != nil {
		log.Fatal(err)
	}
}

func build() error {
	cmd := exec.Command("make")
	return cmd.Run()
}

func runTest() error {
	cwd, err := os.Getwd()
	if err != nil {
		return err
	}
	cmd := exec.Command(path.Join(cwd, "test"), "http://localhost:4711")
	output, err := cmd.CombinedOutput()
	if err != nil {
		return fmt.Errorf("Failed to run test: %s", string(output))
	}
	fmt.Print(string(output))
	return nil
}

func serve() {
	http.HandleFunc("/get", testGETHandler)
	http.HandleFunc("/post", testPOSTHandler)
	http.HandleFunc("/redirect", testRedirectHandler)
	http.HandleFunc("/redirected", redirectedHandler)
	log.Fatal(http.ListenAndServe(":4711", nil))
}

func fail(w http.ResponseWriter, message string) {
	w.WriteHeader(400)
	w.Write([]byte(message + "\n"))
}

func ok(w http.ResponseWriter) {
	w.Write([]byte("OK"))
}

func checkHeader(w http.ResponseWriter, r *http.Request, header string, expected string) bool {
	actual := r.Header[header]
	if len(actual) == 0 || actual[0] != expected {
		fail(w, fmt.Sprintf("Expected header %q to be %q, got %q", header, expected, actual))
		return false
	}
	return true
}

func testGETHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != "GET" {
		fail(w, fmt.Sprintf("Unexpected method, %q", r.Method))
		return
	}

	if r.ContentLength != 0 {
		fail(w, "Non-empty body in GET")
		return
	}

	if !checkHeader(w, r, "Accept", "naett/testresult") {
		return
	}

	if !checkHeader(w, r, "User-Agent", "Naett/1.0") {
		return
	}

	ok(w)
}

func testPOSTHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != "POST" {
		fail(w, fmt.Sprintf("Unexpected method, %q", r.Method))
		return
	}

	if r.ContentLength == 0 {
		fail(w, "Empty body in POST")
		return
	}

	if !checkHeader(w, r, "Accept", "naett/testresult") {
		return
	}

	if !checkHeader(w, r, "User-Agent", "Naett/1.0") {
		return
	}

	bodyBytes, err := ioutil.ReadAll(r.Body)
	if err != nil {
		fail(w, err.Error())
	}

	body := string(bodyBytes)
	expectedBody := "TestRequest!"

	if body != expectedBody {
		fail(w, "Unexpected body")
	}

	ok(w)
}

func testRedirectHandler(w http.ResponseWriter, _ *http.Request) {
	w.Header().Add("Location", "/redirected")
	w.WriteHeader(302)
}

func redirectedHandler(w http.ResponseWriter, _ *http.Request) {
	w.Write([]byte("Redirected"))
}
