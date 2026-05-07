package main

import (
	"fmt"
	"math/rand"
	"strconv"
	"time"
)

// Generate a 4-digit random number
func generateRandomNumber() []int {
	digits := rand.Perm(10)[:4]
	return digits
}

// Get user input as a slice of integers
func getUserGuess() []int {
	var userGuessStr string
	var userGuess []int
	var err error

	for {
		fmt.Print("Enter a 4-digit number: ")
		fmt.Scan(&userGuessStr)

		if len(userGuessStr) != 4 {
			fmt.Println("Invalid input. Please enter a 4-digit number.")
			continue
		}

		userGuess = make([]int, 4)
		for i := 0; i < 4; i++ {
			userGuess[i], err = strconv.Atoi(string(userGuessStr[i]))
			if err != nil {
				fmt.Println("Invalid input. Please enter a 4-digit number.")
				break
			}
		}

		if err == nil {
			break
		}
	}

	return userGuess
}

// Calculate the number of A's and B's in the guess
func calculateScore(guess []int, answer []int) (int, int) {
	a := 0
	b := 0

	for i := 0; i < 4; i++ {
		if guess[i] == answer[i] {
			a++
		} else {
			for j := 0; j < 4; j++ {
				if guess[i] == answer[j] {
					b++
				}
			}
		}
	}

	return a, b
}

// Main game loop
func playGame() {
	answer := generateRandomNumber()
	numGuesses := 0

	for {
		numGuesses++
		guess := getUserGuess()
		a, b := calculateScore(guess, answer)
		fmt.Printf("%dA%dB\n", a, b)

		if a == 4 {
			fmt.Printf("Congratulations! You guessed the answer in %d tries.\n", numGuesses)
			break
		}
	}
}

func main() {
	rand.Seed(time.Now().UnixNano())
	playGame()
}

