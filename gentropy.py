#!/usr/bin/python

import argparse
import re
import math

def check_range(low, high, adjust=0):
  """Helper function for bounded integers in argparse"""
  def ret(val):
    ivalue = int(val)
    if (ivalue < low or ivalue > high):
      raise argparse.ArgumentTypeError("%s needs to be an integer between %i and %i" % (val, low, high))
    return ivalue + adjust
  return ret

check = dict()
check['camelcase'] = re.compile('\A[a-z]+\Z')
check['spaces'] = re.compile('\A[a-zA-Z]+\Z')

def transform(word, style):
  """Transform a word according to a given style"""
  if style == 'camelcase':
    return word[0].upper() + word[1:]
  if style == 'spaces':
    return word + ' '

def load_dictionary(fil, style, costfn, minlen, maxlen):
  """Build a dictionary object (dict (cost -> list of words)) from a file"""
  dic = dict()
  for line in fil:
    word = line.strip()
    if len(word) >= minlen and len(word) <= maxlen and check[style].match(word):
      cost = costfn(word)
      if cost not in dic:
        dic[cost] = set()
      dic[cost].add(transform(word, style))
  for cost in dic:
    dic[cost] = list(dic[cost])
  return dic

def random_int(source, rang):
  """Compute a random integer in the range [0..rang-1]"""
  # This function tries to minimize the number of bytes read from source on
  # average. 
  min_bytes = ((rang - 1).bit_length() + 7) / 8
  best_bytes = 0
  best_avgread = float("inf")
  best_trang = 0
  # Check whether reading slightly more bytes isn't better on average.
  # For example, if range is 256**8 * 0.75, and we read 8 bytes, we have
  # a 25% chance to overshoot the range, and need to read another 2 bytes
  for bytes in range(min_bytes, min_bytes + 5):
    trang = ((256 ** bytes) / rang) * rang
    avgread = bytes / ((trang / (256.0 ** bytes)))
    if avgread < best_avgread:
      best_bytes = bytes
      best_avgread = avgread
      best_trang = trang
  while True:
    ret = 0
    for byte in range(0, bytes):
      ret = ret * 256 + ord(source.read(1))
    if ret < trang:
      return ret % rang

if __name__ == '__main__':
  parser = argparse.ArgumentParser(description="Generate minimal difficulty passphrases with specified security level")
  parser.add_argument('-r', default='/dev/urandom', type=argparse.FileType('r'), metavar='DEV', dest='prng', help='Specify PRNG source (default %(default)s)')
  parser.add_argument('-b', default=80, type=check_range(32, 512), dest='bits', help='Produce phrases with a given security level in bits (min 32, max 512, default %(default)s)')
  parser.add_argument('-s', default='camelcase', choices=['camelcase', 'spaces'], help='Use the specified separation of words (default %(default)s)', dest='style')
  parser.add_argument('-d', default='/usr/share/dict/words', metavar='FILE', type=argparse.FileType('r'), help='Use the specified word list (default %(default)s)', dest='dictionary')
  parser.add_argument('-n', default=1, type=check_range(1, 1000000000), dest='count', help='Produce this many results to choose from (default %(default)s)')
  parser.add_argument('--stats', action='store_true', help='Print out statistics', dest='stats')
  parser.add_argument('--max-length', default=15, type=check_range(1, 256), metavar='N', dest='maxlen', help='Limit the maximum length of dictionary words (default %(default)s)')
  parser.add_argument('--min-length', default=3, type=check_range(1, 256), metavar='N', dest='minlen', help='Limit the minimum length of dictionary words (default %(default)s)')
  parser.add_argument('--cost-per-word', default=2, type=check_range(0, 100), metavar='N', help='Cost per word in a phrase (default %(default)s)', dest='wordcost')
  parser.add_argument('--cost-per-character', default=1, type=check_range(0, 10), metavar='N', help='Cost per character in a phrase (default %(default)s)', dest='charcost')
  parser.add_argument('--cost-range', default=1, type=check_range(0, 100, 1), metavar='N', help='How much difference in cost between solutions is accepted (default %(default)s)', dest='costrange')
  args = parser.parse_args()
  dic = dict()
  costfn = lambda word: args.charcost * len(word) + args.wordcost
  dic = load_dictionary(args.dictionary, args.style, costfn, args.minlen, args.maxlen)
  combinations = [1]
  totalcost = 0
  while sum(combinations[-args.costrange:]) <= args.count * (2 ** args.bits):
    totalcost = totalcost + 1
    comb = 0
    for cost in xrange(1, totalcost + 1):
      if cost in dic:
        comb += len(dic[cost]) * combinations[totalcost - cost]
    combinations.append(comb)
  if args.stats:
    print "# bits=%f cost=[%i..%i] bits_per_cost=%f" % (math.log(sum(combinations[-args.costrange:])) / math.log(2), totalcost - args.costrange, totalcost, math.log(sum(combinations[-args.costrange:])) / (math.log(2) * totalcost))
  for it in xrange(0, args.count):
    num = random_int(args.prng, sum(combinations[-args.costrange:]))
    totalcost = len(combinations) - args.costrange
    while num >= combinations[totalcost]:
      num = num - combinations[totalcost]
      totalcost = totalcost + 1
    out = ""
    outcost = 0
    while outcost < totalcost:
      togocost = totalcost - outcost
      comb = 0
      for cost in xrange(1, togocost + 1):
        if cost in dic:
          newcomb = comb + len(dic[cost]) * combinations[togocost - cost]
        else:
          newcomb = comb
        if comb <= num and newcomb > num:
          num = num - comb
          out += dic[cost][num % len(dic[cost])]
          outcost += cost
          num = num / len(dic[cost])
          break
        else:
          comb = newcomb
    print out
