#/usr/bin/env bash

function enter_number()
{
   local nrfiles
   read -p "Guess nr of files in directory: " nrfiles
   echo $nrfiles
}

while true
do
   echo
   guessed=$(enter_number)

   if [[ ! "$guessed" =~ ^[0-9]+$ ]]
   then
      echo "Please enter only digits from 0..9."
      continue
   fi

   expect=$(ls * | wc -l)
   echo -n "Number $guessed is "
   if [[ "$guessed" = "$expect" ]]
   then
      echo "correct"
      break
   else
      [[ $guessed -lt $expect ]] && echo "too low" || echo "too high"
   fi
done

echo
echo " ... You win ! ..."
echo
