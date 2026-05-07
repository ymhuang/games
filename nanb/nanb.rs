use rand::Rng;
use std::io;

fn generate_random_number() -> [u8; 4] {
    let mut digits: Vec<u8> = (0..10).collect();
    digits.shuffle(&mut rand::thread_rng());
    let answer: [u8; 4] = [digits[0], digits[1], digits[2], digits[3]];
    answer
}

fn get_user_guess() -> [u8; 4] {
    loop {
        let mut user_input = String::new();
        println!("Enter a 4-digit number:");
        io::stdin()
            .read_line(&mut user_input)
            .expect("Failed to read input");

        let user_guess: Vec<u8> = user_input
            .chars()
            .filter_map(|c| c.to_digit(10).map(|d| d as u8))
            .collect();

        if user_guess.len() != 4 {
            println!("Invalid input. Please enter a 4-digit number.");
            continue;
        }

        let mut guess: [u8; 4] = [0; 4];
        guess.copy_from_slice(&user_guess[..4]);

        return guess;
    }
}

fn calculate_score(guess: &[u8; 4], answer: &[u8; 4]) -> (u8, u8) {
    let mut a: u8 = 0;
    let mut b: u8 = 0;

    for i in 0..4 {
        if guess[i] == answer[i] {
            a += 1;
        } else {
            for j in 0..4 {
                if guess[i] == answer[j] {
                    b += 1;
                }
            }
        }
    }

    (a, b)
}

fn play_game() {
    let answer: [u8; 4] = generate_random_number();
    let mut num_guesses: u32 = 0;

    loop {
        num_guesses += 1;
        let guess: [u8; 4] = get_user_guess();
        let (a, b) = calculate_score(&guess, &answer);
        println!("{}A{}B", a, b);

        if a == 4 {
            println!("Congratulations! You guessed the answer in {} tries.", num_guesses);
            break;
        }
    }
}

fn main() {
    println!("Welcome to the 1A2B game!");
    play_game();
}

