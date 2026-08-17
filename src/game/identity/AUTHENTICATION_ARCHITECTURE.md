# Authentication / Account Recovery Architecture

## Current M8E.2.1 boundary

`AccountHandle` is a stable human-entered account identifier, not a display
name and never a gameplay authority ID. Its grammar is deliberately narrow at
the authentication boundary:

- 3..24 characters;
- lowercase ASCII `a-z`, digits `0-9`, `_`, `-`;
- first character is a letter or digit.

Localized/display player names are a separate future field and may use full
Unicode. Keeping Unicode out of the login identifier avoids normalization and
visually-confusable-account problems without limiting the language of the game
or the player's visible name.

The client currently stores one opaque device bearer token in the OS credential
store under the account handle. `SessionHello` carries `accountHandle +
authToken + SignIn/Register`. The server hashes the token immediately and owns
`AccountId -> PlayerId -> ShipInstanceId`; the client cannot select these IDs.

`SIGN IN` is resolution-only. `REGISTER` is the only operation allowed to
create a binding. A known handle presented with a different bearer token is a
credential failure / handle conflict, not a second account.

For development tests the dedicated server exposes `--reset-auth-state`. The
reset is allowed only before gameplay sessions are admitted. While
`AccountRegistry` is RAM-only this mainly makes the test lifecycle explicit;
M8E.3 keeps the same CLI contract and clears the durable account repository
before the server starts accepting clients.

## M8E.3 password credential

Password support belongs to durable account persistence and must not be added as
an unhashed field to the current in-memory registry.

Target first implementation:

- password length: 12..64 characters;
- printable ASCII only for the authentication secret in the first version;
- the UI can generate a 20+ character random password with a CSPRNG;
- generated passwords use upper/lowercase letters, digits and a small safe
  punctuation alphabet;
- server stores a unique salt + password-KDF result, never plaintext and never
  a fast unsalted SHA-256 password hash;
- password hashing is behind `IPasswordHasher` (Argon2id preferred when the
  dependency is introduced);
- password/recovery traffic is not considered Internet-safe until transport
  security (TLS or an equivalent authenticated encrypted channel) exists.

The existing device bearer token remains useful as a remembered-device
credential. Password proof can enroll/rotate a device token instead of forcing
users to type a password on every launch.

## Account recovery

Recovery is a separate security capability, not a special case of gameplay
session admission.

Planned service boundary:

```text
AccountRecoveryService
  -> AccountRepository
  -> PasswordHasher
  -> DeviceCredentialRepository
  -> SessionRegistry (invalidate active sessions)
  -> optional IRecoveryChannel later
```

The baseline recovery mechanism does not require email infrastructure:

1. At durable account registration the server creates a high-entropy recovery
   secret (at least 128 random bits) and shows/returns it once.
2. The server stores only a digest of that recovery secret.
3. `RECOVER ACCOUNT` asks for `AccountHandle + recovery secret`.
4. Successful recovery may set a new password and must rotate/revoke previous
   device bearer tokens.
5. Existing live sessions for the account are invalidated as part of recovery.
6. Recovery attempts are rate-limited and audited server-side.
7. A future email/provider recovery channel can implement `IRecoveryChannel`
   without changing the account/world persistence schema.

If the user loses the password, every enrolled device credential, and the
recovery secret while no external recovery channel is configured, the account
is intentionally unrecoverable. Security questions and administrator-readable
passwords are explicitly not part of the design.

## Full registration record

M8E.3 registration expands the current handle + bearer-token bootstrap into a durable account record. The security and profile fields remain separate so display/profile changes do not rewrite authentication identity.

Target account slices:

```text
AccountIdentity
  AccountId
  AccountHandle
  DisplayName
  PreferredLocale

PasswordCredential
  passwordKdfId
  salt
  passwordDigest
  passwordChangedAt

DeviceCredential[]
  DeviceCredentialId
  secretDigest
  issuedAt / lastUsedAt / revokedAt

RecoveryCredential
  recoverySecretDigest
  optional verified RecoveryContact

ConsentRecord[]
  purpose/version/value/timestamp
```

`DisplayName` is UTF-8/Unicode and is not the login key. It is normalized and moderated through the reusable display-name moderation service before publication. Account handles remain narrow ASCII.

Recovery contact is modeled as a verified channel (`email`, `phone`, later another provider) rather than a free-form gameplay field. Contact data that must later be used to send a recovery message is protected as sensitive account data and is not exposed to gameplay/client replication.

## Remembered-device sign-in and explicit sign-out

A successful password/recovery authentication may mint a per-device long-lived credential when the user chooses to remember the device. Windows stores that secret in Credential Manager; the server stores only its digest and metadata. Ordinary game exit/disconnect keeps it. Explicit **Sign out** revokes/removes that device credential so the next sign-in requires password or recovery.

Runtime gameplay session tokens remain shorter-lived/transient and do not replace the durable remembered-device record. Device credentials are independently revocable, rotatable and auditable.

`ClientPreferencesStore` remembers only non-secret context such as the last successful account handle for each server endpoint. It never stores the password.

## Recovery channels

The one-time high-entropy recovery secret remains the baseline recovery mechanism. A verified email/phone channel can later invoke the same `AccountRecoveryService` through `IRecoveryChannel`; adding a provider must not alter `AccountId`, `PlayerId` or ship ownership.

Recovery must not disclose whether an arbitrary account exists more than necessary, must be rate-limited, and successful recovery revokes/rotates remembered-device credentials and invalidates live sessions.

## Transport/security staging

Password and external recovery-channel deployment to an untrusted network requires authenticated encryption (TLS or equivalent) before being considered production-safe. The current bearer-token development transport is not a justification for sending reusable passwords in plaintext over the public Internet.

## Persistence relationship

Authentication storage is one repository inside the M8E.3 authoritative
persistence subsystem, not a separate temporary database. Durable account data
contains stable identity/security records; world persistence contains mutable
universe facts. They share schema/version/checkpoint/recovery infrastructure but
remain separate repositories.

`EntityId` is never a durable authentication or save identifier.
