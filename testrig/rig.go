package main

import (
	"fmt"
	"io"
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
		log.Fatal(err)
	}

	go serve()

	err = runTest()
	if err != nil {
		log.Fatal(err)
	}

	os.Exit(0)
}

func build() error {
	cmd := exec.Command("make")
	output, err := cmd.CombinedOutput()
	if err != nil {
		return fmt.Errorf("build failed: %v", string(output))
	}
	return nil
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

type Handler func(w http.ResponseWriter, r *http.Request)

func trace(handler Handler) Handler {
	return func(w http.ResponseWriter, r *http.Request) {
		log.Printf("%v - %v", r.Method, r.URL)
		handler(w, r)
	}
}

func serve() {
	http.HandleFunc("/get", trace(testGETHandler))
	http.HandleFunc("/stress", testGETHandler)
	http.HandleFunc("/post", trace(testPOSTHandler))
	http.HandleFunc("/redirect", trace(testRedirectHandler))
	http.HandleFunc("/redirected", trace(redirectedHandler))
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

	bodyBytes, err := io.ReadAll(r.Body)
	if err != nil {
		fail(w, err.Error())
	}

	body := string(bodyBytes)
	expectedBody := "TestRequest!"

	if body != expectedBody {
		fail(w, fmt.Sprintf("Unexpected body: %v", bodyBytes))
	} else {
		ok(w)
	}
}

func testRedirectHandler(w http.ResponseWriter, _ *http.Request) {
	w.Header().Add("Location", "/redirected")
	w.WriteHeader(302)
}

func redirectedHandler(w http.ResponseWriter, _ *http.Request) {
	w.Write([]byte("Redirected"))
}
