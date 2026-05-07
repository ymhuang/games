import random

# Generate a 4-digit random number
def generate_random_number():
    digits = list(range(10))
    random.shuffle(digits)
    return digits[:4]

# Get user input as a list of digits
def get_user_guess():
    while True:
        try:
            user_input = input("Enter a 4-digit number: ")
            user_guess = [int(d) for d in user_input]
            if len(user_guess) != 4:
                raise ValueError
            return user_guess
        except ValueError:
            print("Invalid input. Please enter a 4-digit number.")

# Calculate the number of A's and B's in the guess
def calculate_score(guess, answer):
    a = sum([1 for i in range(4) if guess[i] == answer[i]])
    b = len(set(guess).intersection(answer)) - a
    return (a, b)

# Main game loop
def play_game():
    answer = generate_random_number()
#    print("answer: ", answer)
    num_guesses = 0
    
    while True:
        num_guesses += 1
        guess = get_user_guess()
        score = calculate_score(guess, answer)
        print(f"{score[0]}A{score[1]}B")
        
        if score[0] == 4:
            print(f"Congratulations! You guessed the answer in {num_guesses} tries.")
            break

if __name__ == "__main__":
    play_game()

