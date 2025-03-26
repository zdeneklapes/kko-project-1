# Cryptography project 2

## Implementation of the SHA-256 algorithm and extension length attack

## Author: 
- Zdeněk Lapeš <lapes.zdenek@gmail.com> (xlapes02, 230614)

### Installation

```bash
make RELEASE=1
```

or with debug information

```bash
make
```

## Functionality
All required functionality was implemented and tested as was described in the assignment.

**Examples provided in the assignment were tested and are working as expected:**

```bash
echo -ne "zprava" | ./kry -c
echo -ne "zprava" | ./kry -s -k heslo
echo -ne "zprava" | ./kry -v -k heslo -m 23158796a45a9392951d9a72dffd6a539b14a07832390b937b94a80ddb6dc18e
echo -ne "message" | ./kry -v -k password -m 23158796a45a9392 951d9a72dffd6a539b14a07832390b937b94a80ddb6dc18e
echo -ne "zprava" | ./kry -e -n 5 -a ==message -m 23158796a45a9392951d9a72dffd6a539b14a07832390b937b94a80ddb6dc18e
```


