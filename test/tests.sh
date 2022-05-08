for i in ../test/*.lua; do
  if ! ./apolloc "$i"; then
    echo "Failed to compile '$i'"
    exit
  fi

  if ! ./apollo "$i"; then
    echo "Failed to execute '$i'"
    exit
  fi

  echo "Test '$i' passed"
done
echo "All tests passed"