for i in ../test/*.lua; do
  if ! ./apolloc "$i"; then
    echo "Failed to compile"
    exit
  fi

  if ! ./apollo "$i"; then
    echo "Failed to execute"
    exit
  fi
done
echo "All tests passed"